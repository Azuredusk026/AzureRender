/* Offline sampler using ufbx v0.17.1 (MIT), https://github.com/ufbx/ufbx.
 * Output owns plain JSON samples; no FBX dependency enters the runtime. */
#include "ufbx.h"
#include <stdio.h>
#include <math.h>

static void string_json(ufbx_string value) {
    putchar('"');
    for (size_t i = 0; i < value.length; ++i) {
        unsigned char c = (unsigned char)value.data[i];
        if (c == '"' || c == '\\') putchar('\\');
        if (c < 32) printf("\\u%04x", c); else putchar(c);
    }
    putchar('"');
}
static void transform_json(ufbx_transform t) {
    printf("[[%.10g,%.10g,%.10g],[%.10g,%.10g,%.10g,%.10g],[%.10g,%.10g,%.10g]]",
        t.translation.x,t.translation.y,t.translation.z,
        t.rotation.x,t.rotation.y,t.rotation.z,t.rotation.w,t.scale.x,t.scale.y,t.scale.z);
}
int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr,"Usage: azure_fbx_sample clip.fbx\n"); return 2; }
    ufbx_load_opts opts = {0};
    opts.ignore_geometry = true;
    opts.target_axes = ufbx_axes_right_handed_y_up;
    opts.target_unit_meters = 1.0;
    ufbx_error error;
    ufbx_scene *scene = ufbx_load_file(argv[1], &opts, &error);
    if (!scene) { char text[1024]; ufbx_format_error(text,sizeof(text),&error);
        fprintf(stderr,"FBX load failed: %s\n",text); return 1; }
    printf("{\"parser\":\"ufbx 0.17.1\",\"unitMeters\":1,\"sourceUnitMeters\":%.9g,\"nodes\":[", scene->settings.unit_meters);
    for (size_t i=0;i<scene->nodes.count;++i) {
        ufbx_node *node=scene->nodes.data[i];
        if(i) putchar(','); printf("{\"name\":");string_json(node->name);
        printf(",\"parent\":%d,\"bone\":%s,\"rest\":",node->parent?(int)node->parent->typed_id:-1,node->bone?"true":"false");
        transform_json(node->local_transform); putchar('}');
    }
    printf("],\"clips\":[");
    for(size_t a=0;a<scene->anim_stacks.count;++a) {
        ufbx_anim_stack *stack=scene->anim_stacks.data[a];
        double duration=stack->time_end-stack->time_begin;
        if(duration<=0||duration>600) { fprintf(stderr,"Invalid animation duration\n");ufbx_free_scene(scene);return 1; }
        unsigned frames=(unsigned)ceil(duration*30.0-1e-7)+1;
        if(a)putchar(','); printf("{\"name\":");string_json(stack->name);
        printf(",\"duration\":%.10g,\"sampleRate\":30,\"frames\":[",duration);
        for(unsigned f=0;f<frames;++f) {
            double time=fmin(f/30.0,duration);
            if(f)putchar(',');printf("{\"time\":%.10g,\"transforms\":[",time);
            for(size_t n=0;n<scene->nodes.count;++n) {
                if(n)putchar(',');transform_json(ufbx_evaluate_transform(stack->anim,scene->nodes.data[n],stack->time_begin+time));
            }
            printf("]}");
        }
        printf("]}");
    }
    printf("]}\n");ufbx_free_scene(scene);return 0;
}
