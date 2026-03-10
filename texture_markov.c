#include "texture_markov.h"
#include "fixed.h"
#include "texture.h"
#include "memory.h"
#include "main.h"

#include "win32/w_kernel.h"
#include "win32/w_main.h"

#define COLOR_RANGE 0x100

static int mipmapOffset(int width,int height,int mipmap){
    int offset = 0;
    for(int i = 0;i < mipmap;i++)
        offset += (width >> i) * (height >> i);
    return offset;
}

static bool probabilityCompare(Graph* graph,GraphNode* node,uint8* position){
    int difference = getProbabilityDistance(graph,node,position);
    return difference <= graph->n_difference_threshold;
}

static int getProbabilityProbabilityDistance(Graph* graph,GraphNode* probability_1,GraphNode* probability_2){
#ifdef __wasm__
    int distance = 0;
    
    for(int k = 0;k < DIMENSIONS;k++){
        int rel = (probability_1->position[k]) - (probability_2->position[k]);
        distance += tAbs(rel) * graph->weights_inverse[k] >> 8;
    }
    return distance;
#else
    __m128i acc = _mm_setzero_si128();
    for(int k = 0;k < DIMENSIONS;k += 4){
        __m128i prob1 = _mm_cvtepu8_epi32(_mm_loadu_si128(probability_1->position + k));
        __m128i prob2 = _mm_cvtepu8_epi32(_mm_loadu_si128(probability_2->position + k));
        __m128i rel = _mm_abs_epi32(_mm_sub_epi32(prob1,prob2));
        rel = _mm_srai_epi32(_mm_mullo_epi32(rel,_mm_loadu_si128(graph->weights_inverse + k)),8); 
        acc = _mm_add_epi32(acc,rel);
    }
    __m128i sum1 = _mm_hadd_epi32(acc, acc); 
    __m128i sum2 = _mm_hadd_epi32(sum1, sum1); 

    int distance = _mm_cvtsi128_si32(sum2);
    return distance;
#endif
}

static int getProbabilityDistance(Graph* graph,GraphNode* probability,uint8* position){
#ifdef __wasm__
    int distance = 0;
    for(int k = 0;k < DIMENSIONS;k++){
        int rel = (position[k]) - (probability->position[k]);
        distance += tAbs(rel) * graph->weights_inverse[k] >> 8;
    }
    return distance;
#else
    __m128i acc = _mm_setzero_si128();
    for(int k = 0;k < DIMENSIONS;k += 4){
        __m128i prob = _mm_cvtepu8_epi32(_mm_loadu_si128(probability->position + k));
        __m128i positions = _mm_cvtepu8_epi32(_mm_loadu_si128(position + k));
        __m128i rel = _mm_abs_epi32(_mm_sub_epi32(positions,prob));
        rel = _mm_srai_epi32(_mm_mullo_epi32(rel,_mm_loadu_si128(graph->weights_inverse + k)),8);
        acc = _mm_add_epi32(acc,rel);
    }
    __m128i sum1 = _mm_hadd_epi32(acc, acc); 
    __m128i sum2 = _mm_hadd_epi32(sum1, sum1);

    int distance = _mm_cvtsi128_si32(sum2);
    return distance;
#endif
}

static GraphNode* graphGet(Graph* graph,GraphNode* node,uint8* position){
    if(!node)
        return 0;
    for(;;){
        int distance = getProbabilityDistance(graph,node,position);

        int min_distance = INT_MAX;
        int min_index = -1;

        for(int i = 0;i < countof(node->neighbours);i++){
            if(node->neighbours[i] == -1)
                continue;
            int neighbour_distance = getProbabilityDistance(graph,graph->entry + node->neighbours[i],position);
            if(neighbour_distance < min_distance){
                min_distance = neighbour_distance;
                min_index = i;
            }
        }
        if(min_distance < distance && min_index != -1)
            node = graph->entry + node->neighbours[min_index];
        else
            return node;
    }
}

structure(Candidate){
    int distance;
    GraphNode* node;
};

structure(Heap){
	int amount;
    Candidate candidates[0xFF];
};

static void heapInsert(Heap* heap,Candidate value){
	int index;
	if(heap->amount == countof(heap->candidates)){
        int max_index = (countof(heap->candidates) + 1) / 2 - 1;
        for(int i = max_index + 1;i < heap->amount;i++){
            if(heap->candidates[i].distance > heap->candidates[max_index].distance)
                max_index = i;
        }
        index = max_index;
        if(heap->candidates[index].distance < value.distance)
            return;
    }
	else{
        index = heap->amount;
        heap->amount += 1;  
    }
    heap->candidates[index] = value;
	while(index > 0 && heap->candidates[(index - 1) / 2].distance > heap->candidates[index].distance){
		Candidate temp = heap->candidates[(index - 1) / 2];
		heap->candidates[(index - 1) / 2] = heap->candidates[index];
		heap->candidates[index] = temp;
		index = (index - 1) / 2;
    }   
}

static Candidate heapGet(Heap* heap){
	if(!heap->amount){
        Candidate candidate = {0};
		return candidate;
	}
	Candidate result = heap->candidates[0];
	heap->amount -= 1;
	heap->candidates[0] = heap->candidates[heap->amount];

	int index = 0;
    while(index < heap->amount){
        int maxIndex = index;
        int left = index * 2 + 1;
        int right = index * 2 + 2;

        if(left < heap->amount && heap->candidates[left].distance < heap->candidates[maxIndex].distance)
            maxIndex = left;
        if(right < heap->amount && heap->candidates[right].distance < heap->candidates[maxIndex].distance)
            maxIndex = right;
        if(maxIndex == index) 
            break; 
		
		Candidate temp = heap->candidates[index];
		heap->candidates[index] = heap->candidates[maxIndex];
		heap->candidates[maxIndex] = temp;
        index = maxIndex; 
    }
	return result;
}

static bool candidateInsert(Candidate* candidate_list,int n_candidate,Candidate candidate){
    for(int j = 0;j < n_candidate;j++){
        if(candidate_list[j].node == candidate.node)
            return false;
        if(!candidate_list[j].node || candidate_list[j].distance > candidate.distance){
            for(int k = n_candidate - 2;k >= j;k--)
                candidate_list[k + 1] = candidate_list[k];
            candidate_list[j] = candidate;
            return true;
        }
    }
    return false;
}

static void neighboursGet(Graph* graph,GraphNode* node,Candidate* candidate_list,int n_candidate,int n_search,uint8* position){
    if(!node || !graph->n_nodes)
        return;
    candidate_list[0].node = node;
    candidate_list[0].distance = getProbabilityDistance(graph,node,position);

    Heap heap = {0};

    GraphNode* visited_table[0x100] = {0};

    for(int counter = 0;counter < n_search;counter++){
        for(int i = 0;i < countof(node->neighbours);i++){
            if(node->neighbours[i] == -1)
                continue;
            
            if(visited_table[graph->entry[node->neighbours[i]].hash % countof(visited_table)] == graph->entry + node->neighbours[i])
                continue;
            
            int distance = getProbabilityDistance(graph,graph->entry + node->neighbours[i],position);
            Candidate candidate = {
                .distance = distance,
                .node = graph->entry + node->neighbours[i],
            };
            visited_table[candidate.node->hash % countof(visited_table)] = candidate.node;

            heapInsert(&heap,candidate);
        }
        Candidate candidate = heapGet(&heap);
        if(!candidate.node)
            break;
        candidateInsert(candidate_list,n_candidate,candidate);
        node = candidate.node;
    }
    for(;;){
        Candidate candidate = heapGet(&heap);
        if(!candidate.node)
            return;
        candidateInsert(candidate_list,n_candidate,candidate);
    }
}

static GraphNode* graphInference(Graph* graph,uint8* position){
    GraphNode* node = graphGet(graph,graph->entry,position);
    Candidate candidate_list[0x100] = {0};
    neighboursGet(graph,node,candidate_list,graph->n_candidates_inference,graph->n_search_inference,position);
    node = candidate_list[0].node;
    return node;
}

static void disconnectConnection(Graph* graph,GraphNode* node,GraphNode* neighbour){
    for(int m = 0;m < countof(node->neighbours);m++){
        if(neighbour->neighbours[m] == (int)(node - graph->entry)){
            neighbour->neighbours[m] = -1;
            return;
        }
    }
}

static int getNumberOfNeighbours(GraphNode* node){
    int n = 0;
    for(int i = 0;i < countof(node->neighbours);i++){
        if(node->neighbours[i] == -1)
            continue;
        n += 1;
    }
    return n;
}

static void connectToGraph(Graph* graph,GraphNode* node,uint8* position){
    GraphNode* near_node = graphGet(graph,graph->entry,position);
        
    Candidate candidate_list[0x100];

    for(int i = 0;i < graph->n_candidates_inference;i++){
        candidate_list[i].node = 0;
        candidate_list[i].distance = 0;
    }

    neighboursGet(graph,near_node,candidate_list,graph->n_candidates_inference,graph->n_search_inference,position);

    int neighbour_index = 0;

    for(int i = 0;i < graph->n_candidates_inference;i++){
        if(!candidate_list[i].node)
            break;
        bool reject = false;

        for(int j = 0;j < countof(node->neighbours);j++){
            if(node->neighbours[j] == -1)
                continue;

            int neighbour_distance = fixedMulR(getProbabilityProbabilityDistance(graph,candidate_list[i].node,graph->entry + node->neighbours[j]),FIXED_ONE + FIXED_ONE / 32 * 12);

            if(neighbour_distance < candidate_list[i].distance){
                reject = true;
                break;
            }
        }
        
        if(reject)
            continue;

        bool is_full = true;
        for(int j = 0;j < countof(node->neighbours);j++){
            if(candidate_list[i].node->neighbours[j] == -1){
                candidate_list[i].node->neighbours[j] = (node - graph->entry);
                is_full = false;
                break;
            }
        }
        if(is_full){
            int max_distance = INT_MIN;
            int max_index = -1;
            for(int m = 0;m < countof(node->neighbours);m++){
                if(candidate_list[i].node->neighbours[m] == -1)
                    continue;
                int n_neighbour_neighbours = getNumberOfNeighbours(graph->entry + candidate_list[i].node->neighbours[m]);
                if(n_neighbour_neighbours == 1)
                    continue;
                int distance = getProbabilityDistance(graph,candidate_list[i].node,graph->entry + candidate_list[i].node->neighbours[m]);
                if(distance > max_distance){
                    max_distance = distance;
                    max_index = m;
                }
            }
            if(max_index != -1 && candidate_list[i].distance < max_distance){
                disconnectConnection(graph,candidate_list[i].node,graph->entry + candidate_list[i].node->neighbours[max_index]);
                candidate_list[i].node->neighbours[max_index] = (int)(node - graph->entry);
            }
            else{
                reject = true;
            }
        }

        if(reject)
            continue;

        node->neighbours[neighbour_index] = (int)(candidate_list[i].node - graph->entry);

        neighbour_index += 1;
        if(neighbour_index >= countof(node->neighbours))
            break;
    }
}

static void setGraph(Graph* graph,GraphNode* node,uint8* position){
    if(graph->n_nodes == 1)
        return;
    connectToGraph(graph,node,position);
}

static Graph g_graph_color[0x10];

#define MARGIN 4

static void extractImageColor(int* data,int width,int height,int mipmap,int offset,int amount){
    int mipmap_offset = mipmapOffset(width,height,mipmap);
    int mipmap_width = width >> mipmap;
    int mipmap_height = height >> mipmap;

    for(int i = 0;i < amount;i++){
        int x = (offset + i) / mipmap_width;
        int y = (offset + i) % mipmap_height;
        if(x < MARGIN || x >= mipmap_height - MARGIN || y < MARGIN || y >= mipmap_width - MARGIN)
            continue;
        uint8 position[DIMENSIONS] = {0};

        for(int k = 0;k < countof(offsets);k++){
            int color = data[mipmap_offset + (x + offsets[k][0]) * mipmap_width + (y + offsets[k][1])];
            for(int c = 0;c < 3;c++)
                position[k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        for(int k = 0;k < countof(offsets_up);k++){
            int color = data[mipmap_offset + (x / 2 + offsets_up[k][0]) * (mipmap_width / 2) + (y / 2 + offsets_up[k][1]) + mipmap_width * mipmap_height];
            for(int c = 0;c < 3;c++)
                position[countof(offsets) * 3 + k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        GraphNode* graph_node = graphInference(g_graph_color + mipmap,position);
        if(!graph_node || !probabilityCompare(g_graph_color + mipmap,graph_node,position)){
            GraphNode* graph_node = g_graph_color[mipmap].entry + g_graph_color[mipmap].n_nodes++;
            for(int j = 0;j < DIMENSIONS;j++)
                graph_node->position[j] = position[j];

            graph_node->sample[0].color = data[mipmap_offset + x * mipmap_width + y];
            graph_node->sample[0].n_sample = 1;
            graph_node->hash = tRnd();

            for(int j = 0;j < countof(graph_node->neighbours);j++)
                graph_node->neighbours[j] = -1;

            setGraph(g_graph_color + mipmap,graph_node,position);
        }
        else{
            int color = data[mipmap_offset + x * mipmap_width + y];
            for(int j = 0;j < countof(graph_node->sample);j++){
                if(graph_node->sample[j].color == color || !graph_node->sample[j].n_sample){
                    graph_node->sample[j].color = color;
                    graph_node->sample[j].n_sample += 1;
                    break;
                }
            }
        }
    }
}

static void shuffle(int* arr,int size){
    for(int i = size - 1;i > 0;i--){
        int j = tRnd() % (i + 1);

        int temp = arr[i];
        arr[i] = arr[j];
        arr[j] = temp;
    }
}

structure(PixelFillThreadInfo){
    int* indices;
    int mipmap;
    int image_size;
    int* data;
    int n_threads;
};

static unsigned stdcall generateColorThread(void* arg){
    PixelFillThreadInfo* info = arg;

    int mipmap_offset = 0;
    for(int i = 0;i < info->mipmap;i++)
        mipmap_offset += (info->image_size >> i) * (info->image_size >> i);

    int mipmap_size = info->image_size >> info->mipmap;

    for(int i = 0;i < mipmap_size * mipmap_size / info->n_threads;i++){
        int x = info->indices[i] / mipmap_size;
        int y = info->indices[i] % mipmap_size;

        int luminance[3];
        GraphNode* graph_node;

        uint8 position[DIMENSIONS] = {0};

        for(int k = 0;k < countof(offsets);k++){
            int offset_x = (unsigned)(x + offsets[k][0]) % mipmap_size;
            int offset_y = (unsigned)(y + offsets[k][1]) % mipmap_size;

            int color = info->data[mipmap_offset + offset_x * mipmap_size + offset_y];

            for(int c = 0;c < 3;c++)
                position[k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        for(int k = 0;k < countof(offsets_up);k++){
            int offset_x = (unsigned)(x / 2 + offsets_up[k][0]) % (mipmap_size / 2);
            int offset_y = (unsigned)(y / 2 + offsets_up[k][1]) % (mipmap_size / 2);

            int color = info->data[mipmap_offset + offset_x * (mipmap_size / 2) + offset_y + mipmap_size * mipmap_size];

            for(int c = 0;c < 3;c++)
                position[countof(offsets) * 3 + k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        graph_node = graphInference(g_graph_color + info->mipmap,position);

        if(!graph_node)
            continue;

        int n_sample = 0;

        for(int j = 0;j < countof(graph_node->sample);j++)
            n_sample += graph_node->sample[j].n_sample;

        if(!n_sample)
            continue;

        for(int j = 0;j < countof(graph_node->sample);j++){
            if(tRnd() % n_sample < graph_node->sample[j].n_sample){
                info->data[mipmap_offset + x * mipmap_size + y] = graph_node->sample[j].color;
                break;
            }
            n_sample -= graph_node->sample[j].n_sample;
        }
    }
    return 0;
}

static unsigned stdcall generateColorBruteForceThread(void* arg){
    PixelFillThreadInfo* info = arg;

    int mipmap_offset = 0;
    for(int i = 0;i < info->mipmap;i++)
        mipmap_offset += (info->image_size >> i) * (info->image_size >> i);

    int mipmap_size = info->image_size >> info->mipmap;

    for(int i = 0;i < mipmap_size * mipmap_size / info->n_threads;i++){
        int x = info->indices[i] / mipmap_size;
        int y = info->indices[i] % mipmap_size;

        int luminance[3] = {0};
        GraphNode* graph_node;
    
        uint8 position[DIMENSIONS];
        for(int j = 0;j < DIMENSIONS;j++)
            position[j] = 0;

        for(int k = 0;k < countof(offsets);k++){
            int offset_x = (unsigned)(x + offsets[k][0]) % mipmap_size;
            int offset_y = (unsigned)(y + offsets[k][1]) % mipmap_size;

            int color = info->data[mipmap_offset + offset_x * mipmap_size + offset_y];

            for(int c = 0;c < 3;c++)
                position[k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        for(int k = 0;k < countof(offsets_up);k++){
            int offset_x = (unsigned)(x / 2 + offsets_up[k][0]) % (mipmap_size / 2);
            int offset_y = (unsigned)(y / 2 + offsets_up[k][1]) % (mipmap_size / 2);

            int color = info->data[mipmap_offset + offset_x * (mipmap_size / 2) + offset_y + mipmap_size * mipmap_size];

            for(int c = 0;c < 3;c++)
                position[countof(offsets) * 3 + k * 3 + c] = (color >> c * 8 & 0xFF);
        }
        int m_score = INT_MAX;
        GraphNode* selected = 0;

        for(int i = 0;i < g_graph_color[info->mipmap].n_nodes;i++){
            GraphNode* node = g_graph_color[info->mipmap].entry + i;
            int score = getProbabilityDistance(g_graph_color + info->mipmap,node,position);
            if(score < m_score){
                selected = node;
                m_score = score;
            }
        }

        int min_score = INT_MAX;
        int min_index;
    
        graph_node = selected;

        if(!graph_node){
            info->data[mipmap_offset + x * mipmap_size + y] = 0xFF00FF;
            continue;
        }
    
        int n_sample = 0;
   
        for(int j = 0;j < countof(graph_node->sample);j++)
            n_sample += graph_node->sample[j].n_sample;

        if(!n_sample){
            info->data[mipmap_offset + x * mipmap_size + y] = 0xFF00FF;
            continue;
        }
   
        for(int j = 0;j < countof(graph_node->sample);j++){
            if(tRnd() % n_sample < graph_node->sample[j].n_sample){
                info->data[mipmap_offset + x * mipmap_size + y] = graph_node->sample[j].color;
                break;
            }
            n_sample -= graph_node->sample[j].n_sample;
        }
    }
    return 0;
}


#include "win32/w_opencl.h"

static int g_indices[4096 * 4096];

static int bias(int value,int bias){
    return fixedDivR(value,(fixedMulR((fixedDivR(FIXED_ONE,bias) - FIXED_ONE * 2),(FIXED_ONE - value)) + FIXED_ONE));
}

static void generateColorInit(int* data,int mipmap,int image_size){
    int mipmap_size = image_size >> mipmap;
    if(!mipmap_size)
        return;
    int mipmap_offset = mipmapOffset(image_size,image_size,mipmap);

    for(int i = 0;i < mipmap_size * mipmap_size;i++){
        int x = i / mipmap_size;
        int y = i % mipmap_size;
        if(data[mipmap_offset + i] >> 24)   
            continue;
        Vec3 up = pixelColorToColor(data[mipmap_offset + (x / 2) * (mipmap_size / 2) + y / 2 + mipmap_size * mipmap_size]);
        Vec3 rnd = {tRnd() % (FIXED_ONE << 4),tRnd() % (FIXED_ONE << 4),tRnd() % (FIXED_ONE << 4)};
        int weight = bias(tRnd() % FIXED_ONE,FIXED_ONE / 100 * 99);
        data[mipmap_offset + i] = colorToPixelColor(vec3Mix(up,rnd,weight));
    }
    for(int i = 0;i < mipmap_size * mipmap_size;i++)
        g_indices[i] = i;
    shuffle(g_indices,mipmap_size * mipmap_size);
}

static void generateColor(int* data,int mipmap,int image_size){
    for(int i = 0;i < tMin(1 << mipmap * 2,256);i++){
        int mipmap_size = image_size >> mipmap;
        if(!mipmap_size)
            return;
        int mipmap_offset = mipmapOffset(image_size,image_size,mipmap);

        PixelFillThreadInfo thread_info[0x10];
        void* threads[0x10];

        int n_threads = tMin(g_n_threads,0x10);
        for(int j = 0;j < n_threads;j++){
            thread_info[j].indices = g_indices + mipmap_size * mipmap_size / n_threads * j;
            thread_info[j].mipmap = mipmap;
            thread_info[j].image_size = image_size;
            thread_info[j].data = data;
            thread_info[j].n_threads = n_threads;
            threads[j] = CreateThread(0,0,generateColorThread,thread_info + j,0,0);
        }
        WaitForMultipleObjects(n_threads,threads,true,-1);
        for(int j = 0;j < n_threads;j++)
            CloseHandle(threads[j]);
    }
}
#include "win32/w_console.h"
void markovTextureTrain(Texture texture){
#if defined(_DEBUG_FAST) || defined(RELEASE_FASTSTARTUP)
    return;
#endif
    int t = __rdtsc() >> 24;
 
    int weights[] = {
        FIXED_ONE,FIXED_ONE,FIXED_ONE,FIXED_ONE,
        FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,
        FIXED_ONE,FIXED_ONE,FIXED_ONE,FIXED_ONE,
        FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,FIXED_ONE + FIXED_ONE / 10 * 2,
    };
    for(int i = 0;i < 0x10;i++){
        for(int j = 0;j < countof(weights);j++){
            for(int c = 0;c < 3;c++)
                g_graph_color[i].weights_inverse[j * 3 + c] = fixedDivR(FIXED_ONE,weights[j]) >> 4;
        }
        g_graph_color[i].n_candidates_inference = 0x10;
        g_graph_color[i].n_search_inference = 0x10;
        g_graph_color[i].n_neighbours = 16;
        g_graph_color[i].n_difference_threshold = 0x400;
    }
    for(int mipmap = 0;mipmap < 0x10;mipmap++)
        extractImageColor(texture.pixel_data,texture.size,texture.size,mipmap,0,(texture.size >> mipmap) * (texture.size >> mipmap));
    print("training time: ");
    printNumberNL((__rdtsc() >> 24) - t);

    int n_neighbours = 0;

    for(int i = 0;i < g_graph_color[0].n_nodes;i++){
        bool connections = false; 
        for(int j = 0;j < 0x10;j++){
            if(g_graph_color[0].entry[i].neighbours[j] == -1)
                continue;
            connections = true;
            bool bidirectional = false;

            for(int k = 0;k < 0x10;k++){
                if(g_graph_color[0].entry[g_graph_color[0].entry[i].neighbours[j]].neighbours[k] == i){
                    bidirectional = true;
                }
            }

            if(!bidirectional){
                print("not bidirectional\n");
            }
            n_neighbours += 1;
        }
        if(!connections){
            print("not connected\n");
        }
    }
    
    print("nodes = ");
    printNumberNL(g_graph_color[0].n_nodes);
    print("neigbours = ");
    printNumberNL(n_neighbours);
    print("avg neigbours = ");
    printNumberNL(n_neighbours / g_graph_color[0].n_nodes);
}

static void markovRandomSampler(Texture texture,Texture sampler){
    int mipmap_offset = 0;
    for(int i = 0;i < 4;i++)
        mipmap_offset += (sampler.size >> i) * (sampler.size >> i);
    int sampler_size = sampler.size >> 4;
    int texture_size = texture.size >> 4;

    int mipmap_offset_texture = 0;
    for(int i = 0;i < 4;i++)
        mipmap_offset_texture += (texture.size >> i) * (texture.size >> i);

    for(int i = 0;i < texture_size * texture_size;i++)
        texture.pixel_data[mipmap_offset_texture + i] = sampler.pixel_data[mipmap_offset + tRnd() % (sampler_size * sampler_size)];
}

void markovTextureGenerate(Texture texture,Texture sampler){
#if defined(_DEBUG_FAST) || defined(RELEASE_FASTSTARTUP)
    return;
#endif
    for(int i = 0;i < texture.size * texture.size * 2;i++)
        texture.pixel_data[i] = tRnd();

    int t = __rdtsc() >> 24;
    markovInferenceInitOpenCL(texture,g_graph_color,texture.size);

    for(int i = 0;i < 0x10;i++){
        g_graph_color[i].n_candidates_inference = 0x100;
        g_graph_color[i].n_search_inference = 16;
    }

    for(int mipmap = 0x10;mipmap--;){
        int mipmap_size = texture.size >> mipmap;
        if(mipmap_size < 4)
            continue;
        generateColorInit(texture.pixel_data,mipmap,texture.size);
        /*
        if(texture.size >> mipmap >= 256)
            markovInferenceOpenCL(texture,g_graph_color,g_indices,mipmap,texture.size,16,g_graph_color[mipmap].n_nodes);
        else
        */
            //generateColor(texture.pixel_data,mipmap,texture.size);
        
        if(texture.size >> mipmap >= 256)
            markovInferenceOpenCL(texture,g_graph_color,g_indices,mipmap,texture.size,16,g_graph_color[mipmap].n_nodes);
        else
            generateColor(texture.pixel_data,mipmap,texture.size);
       
        //markovInferenceOpenCL(texture,g_graph_color,g_indices,mipmap,texture.size,16,g_graph_color[mipmap].n_nodes);
    }
    printNumberNL((__rdtsc() >> 24) - t);
}

void markovTextureRemix(Texture texture,int remix_mipmap){
#if defined(_DEBUG_FAST) || defined(RELEASE_FASTSTARTUP)
    return;
#endif 
    int t = __rdtsc() >> 24;
    markovInferenceInitOpenCL(texture,g_graph_color,texture.size);

    for(int mipmap = remix_mipmap;mipmap--;){
        g_graph_color[mipmap].n_candidates_inference = 16;
        g_graph_color[mipmap].n_search_inference = 16;
        generateColorInit(texture.pixel_data,mipmap,texture.size);

        //markovInferenceBruteforceOpenCL(texture,g_graph_color,g_indices,mipmap,texture.size,g_graph_color[mipmap].n_nodes);
        
        if(texture.size >> mipmap >= 256)
            markovInferenceOpenCL(texture,g_graph_color,g_indices,mipmap,texture.size,16,g_graph_color[mipmap].n_nodes);
        else
            generateColor(texture.pixel_data,mipmap,texture.size);
            
    }
    printNumberNL((__rdtsc() >> 24) - t);
    markovInferenceDeInitOpenCL();
}

void markovInit(void){
#if defined(_DEBUG_FAST) || defined(RELEASE_FASTSTARTUP)
    return;
#endif
    for(int i = 0;i < 0x10;i++)
        g_graph_color[i].entry = tMallocZero((sizeof(GraphNode) * 0x1000000) >> i * 2);
}

void markovFree(void){
#if defined(_DEBUG_FAST) || defined(RELEASE_FASTSTARTUP)
    return;
#endif
    for(int i = 0;i < 0x10;i++){
        tFree(g_graph_color[i].entry);
        g_graph_color[i].entry = 0;
        g_graph_color[i].n_nodes = 0;
    }
    markovInferenceDeInitOpenCL();
}
