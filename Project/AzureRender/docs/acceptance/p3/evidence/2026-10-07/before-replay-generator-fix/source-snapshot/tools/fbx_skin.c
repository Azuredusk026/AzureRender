/* Offline source-weight reader using hash-pinned ufbx 0.17.1 (MIT). */
#include "ufbx.h"
#include <stdio.h>
static void str(ufbx_string s) {
    putchar('"');
    for(size_t i=0;i<s.length;++i) {
        unsigned char c=(unsigned char)s.data[i];
        if(c=='"'||c=='\\')putchar('\\');
        if(c<32)printf("\\u%04x",c);else putchar(c);
    }
    putchar('"');
}
int main(int argc,char **argv) {
    if(argc!=2)return 2;
    ufbx_load_opts opts={0}; opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;
    ufbx_error error;ufbx_scene *scene=ufbx_load_file(argv[1],&opts,&error);
    if(!scene){char message[1024];ufbx_format_error(message,sizeof(message),&error);fprintf(stderr,"%s\n",message);return 1;}
    printf("{\"parser\":\"ufbx 0.17.1\",\"meshes\":[");size_t emitted=0;
    for(size_t n=0;n<scene->nodes.count;++n){
        ufbx_node *node=scene->nodes.data[n];ufbx_mesh *mesh=node->mesh;
        if(!mesh||!mesh->skin_deformers.count)continue;
        ufbx_skin_deformer *skin=mesh->skin_deformers.data[0];
        if(emitted++)putchar(',');printf("{\"name\":");str(node->name);printf(",\"bones\":[");
        for(size_t b=0;b<skin->clusters.count;++b){if(b)putchar(',');str(skin->clusters.data[b]->bone_node->name);}
        printf("],\"vertices\":[");
        for(size_t v=0;v<mesh->num_vertices;++v){
            if(v)putchar(',');ufbx_vec3 p=ufbx_transform_position(&node->geometry_to_world,mesh->vertices.data[v]);
            printf("{\"position\":[%.10g,%.10g,%.10g],\"weights\":[",p.x,p.y,p.z);
            ufbx_skin_vertex sv=skin->vertices.data[v];
            for(uint32_t w=0;w<sv.num_weights;++w){if(w)putchar(',');ufbx_skin_weight sw=skin->weights.data[sv.weight_begin+w];printf("[%u,%.10g]",sw.cluster_index,sw.weight);}
            printf("]}");
        }
        printf("],\"corners\":[");
        for(size_t i=0;i<mesh->num_indices;++i){
            if(i)putchar(',');ufbx_vec2 uv=mesh->vertex_uv.exists?mesh->vertex_uv.values.data[mesh->vertex_uv.indices.data[i]]:(ufbx_vec2){0};
            printf("[%u,%.10g,%.10g]",mesh->vertex_indices.data[i],uv.x,uv.y);
        }
        printf("]}");
    }
    printf("]}\n");ufbx_free_scene(scene);return emitted?0:1;
}
