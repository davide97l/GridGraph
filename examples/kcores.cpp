#include "core/graph.hpp"

/*This algorithm works only on undirected graphs*/

int main(int argc, char ** argv) {
	if (argc<4) {
		fprintf(stderr, "usage: kcores [path] [k] [memory budget in GB]\n");
		exit(-1);
	}
	std::string path = argv[1];
	int k = atoi(argv[2]);
	long memory_bytes = (argc>=4)?atol(argv[3])*1024l*1024l*1024l:8l*1024l*1024l*1024l;

	Graph graph(path);
	graph.set_memory_bytes(memory_bytes);
	BigVector<VertexId> degree(graph.path+"/degree", graph.vertices);
	BigVector<bool> removed(graph.path+"/removed", graph.vertices);

	long vertex_data_bytes = (long)graph.vertices * ( sizeof(VertexId) + sizeof(bool));
	graph.set_vertex_data_bytes(vertex_data_bytes);

	double begin_time = get_time();

	degree.fill(0);
	graph.stream_edges<VertexId>(
		[&](Edge & e){
			write_add(&degree[e.source], 1);
			return 0;
		}, nullptr, 0, 0
	);
	printf("degree calculation used %.2f seconds\n", get_time() - begin_time);
	fflush(stdout);

	removed.fill(0);

	bool has_removed = true;

	int iter = 0;
	while (has_removed){
        has_removed = false;
        graph.hint(removed, degree);
        graph.stream_vertices<VertexId>(
            [&](VertexId i){
                if (!removed[i] && degree[i] < k){
                    removed[i] = 1;
                    has_removed = true;
                }
                return 0;
            }, nullptr, 0,
            [&](std::pair<VertexId,VertexId> vid_range){
                removed.load(vid_range.first, vid_range.second);
                degree.load(vid_range.first, vid_range.second);
            },
            [&](std::pair<VertexId,VertexId> vid_range){
                removed.save();
                degree.save();
            }
        );
        if (!has_removed)
            break;
        else{
            graph.stream_edges<VertexId>(
                [&](Edge & e){
                    if (!removed[e.source] && !removed[e.target])
                        write_add(&degree[e.source], 1);
                        write_add(&degree[e.target], 1);
                    return 0;
                }, nullptr, 0, 0
            );
        }
        iter++;
	}

	int removed_vertices = 0;
	for(int i=0;i<(long)graph.vertices;i++){
        if(removed[i]==1)
            removed_vertices++;
	}

	double end_time = get_time();
	printf("kcores took %.2f seconds, removed %d vertices in %d iterations\n", end_time - begin_time,
                                                                               removed_vertices,
                                                                               iter+1);

}


