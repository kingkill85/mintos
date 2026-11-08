#include "core.h"
#include "paths.h"
#include "archive.h"
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- log ---------- */
static FILE *logf_;
void rg_log(const char *fmt, ...) {
    if (!logf_) {
        char p[1024];
        const char *r = getenv("RGARCH_ROOT");
        snprintf(p, sizeof p, "%s/var/log/rgarch.log", r ? r : "");
        mkdirs_for(p);
        logf_ = fopen(p, "w");
        if (!logf_) logf_ = stderr;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logf_, fmt, ap);
    va_end(ap);
    fflush(logf_);
}

/* ---------- core symbols ---------- */
static struct {
    void (*init)(void), (*deinit)(void), (*run)(void), (*reset)(void), (*unload_game)(void);
    void (*get_system_info)(struct retro_system_info *), (*get_system_av_info)(struct retro_system_av_info *);
    void (*set_environment)(retro_environment_t), (*set_video_refresh)(retro_video_refresh_t);
    void (*set_audio_sample)(retro_audio_sample_t), (*set_audio_sample_batch)(retro_audio_sample_batch_t);
    void (*set_input_poll)(retro_input_poll_t), (*set_input_state)(retro_input_state_t);
    void (*set_controller_port_device)(unsigned, unsigned);
    bool (*load_game)(const struct retro_game_info *);
    size_t (*serialize_size)(void);
    bool (*serialize)(void *, size_t), (*unserialize)(const void *, size_t);
    void *(*get_memory_data)(unsigned);
    size_t (*get_memory_size)(unsigned);
} f;

static Core *cur;
static char sysdir[1024], savedir[1024], optfile[1024];
static void *romdata;

/* ---------- options ---------- */
#define MAXOPT 256
typedef struct { char *key, *value; } Opt;
static Opt opts[MAXOPT], file_opts[MAXOPT];
static int nopts, nfile;

static void opt_free(Opt *o, int *n) {
    for (int i = 0; i < *n; i++) { free(o[i].key); free(o[i].value); }
    *n = 0;
}

static void opts_read_file(void) {
    opt_free(file_opts, &nfile);
    FILE *fp = fopen(optfile, "r");
    char line[1024], key[256], val[768];
    while (fp && fgets(line, sizeof line, fp) && nfile < MAXOPT)
        if (sscanf(line, " %255[^ =] = \"%767[^\"]\"", key, val) == 2) {
            file_opts[nfile].key = strdup(key);
            file_opts[nfile++].value = strdup(val);
        }
    if (fp) fclose(fp);
}

/* define an option: value from the .opt file if it is one of the allowed values */
static void opt_define(const char *key, const char *def, const char *const *values, int nvalues) {
    if (nopts >= MAXOPT) return;
    const char *v = def;
    for (int i = 0; i < nfile; i++)
        if (!strcmp(file_opts[i].key, key))
            for (int j = 0; j < nvalues; j++)
                if (!strcmp(values[j], file_opts[i].value)) v = values[j];
    opts[nopts].key = strdup(key);
    opts[nopts++].value = strdup(v ? v : "");
}

static void opts_v1(const struct retro_core_option_definition *d) {
    for (; d && d->key; d++) {
        const char *vals[RETRO_NUM_CORE_OPTION_VALUES_MAX];
        int n = 0;
        while (n < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[n].value) { vals[n] = d->values[n].value; n++; }
        opt_define(d->key, d->default_value ? d->default_value : n ? vals[0] : "", vals, n);
    }
}

static void opts_v2(const struct retro_core_options_v2 *o) {
    for (const struct retro_core_option_v2_definition *d = o ? o->definitions : NULL; d && d->key; d++) {
        const char *vals[RETRO_NUM_CORE_OPTION_VALUES_MAX];
        int n = 0;
        while (n < RETRO_NUM_CORE_OPTION_VALUES_MAX && d->values[n].value) { vals[n] = d->values[n].value; n++; }
        opt_define(d->key, d->default_value ? d->default_value : n ? vals[0] : "", vals, n);
    }
}

/* legacy "Description; a|b|c" */
static void opts_legacy(const struct retro_variable *v) {
    for (; v && v->key; v++) {
        const char *semi = strchr(v->value, ';');
        char buf[2048];
        snprintf(buf, sizeof buf, "%s", semi ? semi + 1 : v->value);
        const char *vals[128];
        int n = 0;
        for (char *t = strtok(buf, "|"); t && n < 128; t = strtok(NULL, "|")) {
            while (*t == ' ') t++;
            vals[n++] = t;
        }
        opt_define(v->key, n ? vals[0] : "", vals, n);
    }
}

const char *core_option(Core *c, const char *key) {
    (void)c;
    for (int i = 0; i < nopts; i++)
        if (!strcmp(opts[i].key, key)) return opts[i].value;
    return NULL;
}

static void opts_write_file(void) {
    if (!nopts) return;
    mkdirs_for(optfile);
    FILE *fp = fopen(optfile, "w");
    if (!fp) return;
    for (int i = 0; i < nopts; i++) fprintf(fp, "%s = \"%s\"\n", opts[i].key, opts[i].value);
    fclose(fp);
}

/* ---------- callbacks ---------- */
static void log_cb(enum retro_log_level l, const char *fmt, ...) {
    static const char *lv[] = {"debug", "info", "warn", "error"};
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    rg_log("[core %s] %s", lv[l & 3], buf);
}

static bool environment(unsigned cmd, void *data) {
    switch (cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL) {
    case RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY:
    case RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY:
        *(const char **)data = sysdir; return true;
    case RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY:
        *(const char **)data = savedir; return true;
    case RETRO_ENVIRONMENT_SET_PIXEL_FORMAT:
        cur->fmt = *(const enum retro_pixel_format *)data; return true;
    case RETRO_ENVIRONMENT_SET_GEOMETRY:
        cur->av.geometry = *(const struct retro_game_geometry *)data;
        cur->geometry_changed = true; return true;
    case RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO:
        cur->av = *(const struct retro_system_av_info *)data;
        cur->geometry_changed = true; return true;
    case RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION:
        *(unsigned *)data = 2; return true;
    case RETRO_ENVIRONMENT_SET_VARIABLES:
        opts_legacy(data); return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS:
        opts_v1(data); return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL:
        opts_v1(((const struct retro_core_options_intl *)data)->us); return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2:
        opts_v2(data); return true;
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2_INTL:
        opts_v2(((const struct retro_core_options_v2_intl *)data)->us); return true;
    case RETRO_ENVIRONMENT_GET_VARIABLE: {
        struct retro_variable *v = data;
        v->value = core_option(cur, v->key);
        return v->value != NULL;
    }
    case RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE:
        *(bool *)data = false; return true;
    case RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK:
        cur->keyboard = ((const struct retro_keyboard_callback *)data)->callback; return true;
    case RETRO_ENVIRONMENT_GET_LOG_INTERFACE:
        ((struct retro_log_callback *)data)->log = log_cb; return true;
    case RETRO_ENVIRONMENT_GET_LANGUAGE:
        *(unsigned *)data = RETRO_LANGUAGE_GERMAN; return true;
    case RETRO_ENVIRONMENT_GET_INPUT_BITMASKS:
    case RETRO_ENVIRONMENT_GET_CAN_DUPE:
        if (data) *(bool *)data = true;
        return true;
    case RETRO_ENVIRONMENT_GET_AUDIO_VIDEO_ENABLE:
        *(int *)data = 3; return true;
    case RETRO_ENVIRONMENT_SET_MESSAGE:
        rg_log("[core msg] %s\n", ((const struct retro_message *)data)->msg); return true;
    case RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME:
    case RETRO_ENVIRONMENT_SET_SERIALIZATION_QUIRKS:
    case RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL:
    case RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS:
    case RETRO_ENVIRONMENT_SET_CONTROLLER_INFO:
    case RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO:
    case RETRO_ENVIRONMENT_SET_MEMORY_MAPS:
    case RETRO_ENVIRONMENT_SET_CORE_OPTIONS_DISPLAY:
    case RETRO_ENVIRONMENT_SET_MINIMUM_AUDIO_LATENCY:
    case RETRO_ENVIRONMENT_SET_CONTENT_INFO_OVERRIDE:
        return true;
    default: {
        static unsigned char seen[128];
        unsigned k = cmd & ~RETRO_ENVIRONMENT_EXPERIMENTAL;
        if (k < sizeof seen * 8 && !(seen[k / 8] & 1 << (k % 8))) {
            seen[k / 8] |= 1 << (k % 8);
            rg_log("environment %u (%s) not supported\n", k, cmd & RETRO_ENVIRONMENT_EXPERIMENTAL ? "experimental" : "stable");
        }
        return false;
    }
    }
}

static void video_cb(const void *d, unsigned w, unsigned h, size_t pitch) { if (cur->on_video) cur->on_video(d, w, h, pitch); }
static void audio_cb(int16_t l, int16_t r) { int16_t s[2] = {l, r}; if (cur->on_audio) cur->on_audio(s, 1); }
static size_t audio_batch_cb(const int16_t *d, size_t n) { return cur->on_audio ? cur->on_audio(d, n) : n; }
static void poll_cb(void) {}
static int16_t input_cb(unsigned p, unsigned d, unsigned i, unsigned id) { return cur->on_input ? cur->on_input(p, d, i, id) : 0; }

/* ---------- lifecycle ---------- */
#define SYM(name) (*(void **)&f.name = dlsym(c->dl, "retro_" #name))

int core_load(Core *c, const char *so, const char *rom) {
    cur = c;
    c->dl = dlopen(so, RTLD_NOW | RTLD_LOCAL);
    if (!c->dl) { rg_log("dlopen %s: %s\n", so, dlerror()); return -1; }
    SYM(init); SYM(deinit); SYM(run); SYM(reset); SYM(unload_game); SYM(get_system_info);
    SYM(get_system_av_info); SYM(set_environment); SYM(set_video_refresh); SYM(set_audio_sample);
    SYM(set_audio_sample_batch); SYM(set_input_poll); SYM(set_input_state); SYM(set_controller_port_device);
    SYM(load_game); SYM(serialize_size); SYM(serialize); SYM(unserialize); SYM(get_memory_data); SYM(get_memory_size);
    if (!f.init || !f.run || !f.load_game || !f.set_environment) { rg_log("%s is not a libretro core\n", so); return -1; }

    f.get_system_info(&c->info);
    const char *root = getenv("RGARCH_ROOT") ? getenv("RGARCH_ROOT") : "";
    char sys[256] = "unknown";
    path_system(rom, sys, sizeof sys);
    snprintf(sysdir, sizeof sysdir, "%s/storage/roms/bios", root);
    snprintf(savedir, sizeof savedir, "%s/storage/saves/%s", root, sys);
    char probe[1100];
    snprintf(probe, sizeof probe, "%s/x", savedir);
    mkdirs_for(probe);   /* cores write BRAM/FDS saves there themselves */
    snprintf(optfile, sizeof optfile, "%s/storage/.config/retroarch/config/%s/%s.opt", root,
             c->info.library_name, c->info.library_name);
    opt_free(opts, &nopts);
    opts_read_file();
    c->fmt = RETRO_PIXEL_FORMAT_0RGB1555;

    f.set_environment(environment);
    f.set_video_refresh(video_cb);
    f.set_audio_sample(audio_cb);
    f.set_audio_sample_batch(audio_batch_cb);
    f.set_input_poll(poll_cb);
    f.set_input_state(input_cb);
    f.init();

    static char loadpath[1024];
    if (archive_prepare(rom, c->info.valid_extensions, loadpath, sizeof loadpath)) return -1;
    struct retro_game_info g = {.path = loadpath};
    if (!c->info.need_fullpath) {
        FILE *fp = fopen(loadpath, "rb");
        if (!fp) { rg_log("cannot read %s\n", loadpath); return -1; }
        fseek(fp, 0, SEEK_END);
        long n = ftell(fp);
        rewind(fp);
        romdata = malloc(n > 0 ? n : 1);
        g.size = fread(romdata, 1, n, fp);
        g.data = romdata;
        fclose(fp);
    }
    if (!f.load_game(&g)) { rg_log("%s: retro_load_game failed for %s\n", c->info.library_name, rom); return -1; }
    f.get_system_av_info(&c->av);
    if (f.set_controller_port_device) f.set_controller_port_device(0, RETRO_DEVICE_JOYPAD);
    c->can_state = f.serialize_size && f.serialize_size() > 0;
    rg_log("loaded %s %s: %ux%u %.2f fps %.0f Hz, states %s\n", c->info.library_name, c->info.library_version,
           c->av.geometry.base_width, c->av.geometry.base_height, c->av.timing.fps, c->av.timing.sample_rate,
           c->can_state ? "yes" : "no");
    return 0;
}

void core_run(Core *c) { cur = c; f.run(); }
void core_reset(Core *c) { cur = c; if (f.reset) f.reset(); }

void core_unload(Core *c) {
    cur = c;
    opts_write_file();
    if (f.unload_game) f.unload_game();
    if (f.deinit) f.deinit();
    free(romdata);
    romdata = NULL;
    opt_free(opts, &nopts);
    opt_free(file_opts, &nfile);
    /* not dlclose'd: several cores do not survive being unloaded and loaded again */
    memset(&f, 0, sizeof f);
}

size_t core_state_size(Core *c) { (void)c; return f.serialize_size ? f.serialize_size() : 0; }
bool core_state_save(Core *c, void *b, size_t n) { (void)c; return f.serialize && f.serialize(b, n); }
bool core_state_load(Core *c, const void *b, size_t n) { (void)c; return f.unserialize && f.unserialize(b, n); }

void *core_memory(Core *c, unsigned id, size_t *size) {
    (void)c;
    *size = f.get_memory_size ? f.get_memory_size(id) : 0;
    return *size && f.get_memory_data ? f.get_memory_data(id) : NULL;
}
