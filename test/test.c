#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <math.h>
#include <assert.h>
#include <alloc/arena.h>
#include <json/json.h>

#include "plot/plot.h"

#pragma pack(push, 1)
typedef struct {
    uint16_t type;            // BM
    uint32_t size;            // Velikost souboru
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offset;          // Offset dat pixelů
    uint32_t header_size;     // Velikost hlavičky BMP
    int32_t  width;           // Šířka obrázku
    int32_t  height;          // Výška obrázku
    uint16_t planes;          // Počet barevných rovin (vždy 1)
    uint16_t bits_per_pixel;  // Počet bitů na pixel (24 pro true color)
    uint32_t compression;     // Typ komprese (0 pro nekomprimovaný BMP)
    uint32_t image_size;      // Velikost dat pixelů
    int32_t  x_pixels_per_meter;
    int32_t  y_pixels_per_meter;
    uint32_t colors_used;     // Počet barev v palete (0 pro true color)
    uint32_t important_colors;
} BMPHeader;
#pragma pack(pop)


bool write_as_bmp(Plot * image, char * path) {
	FILE * f = fopen(path, "w");

	if(f == NULL) {
		printf("Can't open file :%s\n", path);
		return false;
	}

	BMPHeader header = {
		.type = 0x4D42,            // BM
		.size = sizeof(BMPHeader) + (image->size * sizeof(uint32_t)),  // Velikost souboru
		.reserved1 = 0,
		.reserved2 = 0,
		.offset = sizeof(BMPHeader),  // Offset dat pixelů
		.header_size = 40,           // Velikost hlavičky BMP
		.width = image->width,
		.height = image->height,
		.planes = 1,
		.bits_per_pixel = 32,        // 24 bitů na pixel pro true color
		.compression = 0,
		.image_size = image->size * sizeof(uint32_t),  // Velikost dat pixelů
		.x_pixels_per_meter = 0,
		.y_pixels_per_meter = 0,
		.colors_used = 0,            // Pro true color není paleta
		.important_colors = 0
	 };

	fwrite(&header, 1, sizeof(BMPHeader), f);
	fwrite(image->pixel, 1, image->size * sizeof(uint32_t), f);
	fclose(f);

	return true;
}


typedef struct {
    size_t size;
    double * value;
} Vector;


static void vector_iterator_reset(Iterator * it) {
    it->index = 0;
}


static void * vector_iterator_next(Iterator * it) {
    Vector * vector = it->context;
    if(it->index < vector->size) {
        return &vector->value[it->index];
    } else {
        return NULL;
    }
}


static Iterator vector_to_iterator(Vector * self) {
    return (Iterator) {
        .context = self
        , .__reset__ = vector_iterator_reset
        , .__next__ = vector_iterator_next
    };
}


typedef struct {
    bool is_value;
    Vector value;
}O_Batch; 


O_Batch __filter_batch(Json_Array * json, Alloc * alloc, char * key) {
    Vector vec = {.size = JSON_ARRAY(json)->size, .value = new(alloc, sizeof(double) * JSON_ARRAY(json)->size)};
    
    iterate(json_array_iterator(json), Json_Object *, node, {
        Json * value = json_object_at(node, key);
        assert(json_is_type(value, JsonType_Number) == true);
        vec.value[iterator.index] = strtod(JSON_VALUE(value)->c_str, NULL);
    }); 

    return (O_Batch) {.is_value = true, .value = vec};
}


Json * __load_dataset(Alloc * alloc, char * filename) {
    FILE * f = fopen(filename, "r");

	assert(f != NULL);

    fseek(f, 0L, SEEK_END);
    size_t length = ftell(f);
    fseek(f, 0L, SEEK_SET);

    char * json_string = new(alloc, length + 1);
    assert(json_string != NULL);

    fread(json_string, sizeof(char), length, f);
	fclose(f);
    
    Json * json = json_deserialize(alloc, json_string);

    assert(json_is_type(json, JsonType_Array) == true);

	return json;
}

#define ARENA_HEAP_SIZE 1024 * 10000

bool __show_candle_chart(void) {
    ArenaAlloc alloc = arena_alloc(ARENA_HEAP_SIZE);

	Json * json = __load_dataset(ALLOC(&alloc), "dataset/BITCOIN_M1_500.json");
    if(json == NULL) {
        return false;
    }

	O_Batch batch_open = __filter_batch(JSON_ARRAY(json), ALLOC(&alloc), "high");
	O_Batch batch_close = __filter_batch(JSON_ARRAY(json), ALLOC(&alloc), "low");

    if(batch_open.is_value == false || batch_close.is_value == false) {
        return true;
	}

    Iterator range_open = range(&(Range){.start = 0, .end = batch_open.value.size, .step = 1});
    Iterator range_close = range(&(Range){.start = 0, .end = batch_close.value.size, .step = 1});

    ScatterPlot_Series serie_open = {
        .line_type = Plot_LineType_Solid
        , .xs = range_open  
        , .ys = vector_to_iterator(&batch_open.value)
        , .legenda = "High price" 
        , .line_thickness = 2
        , .color = RGBA(.R=0xFF, .G=0x00, .B=0x20, .A=0xF0)};

    ScatterPlot_Series serie_close = {
        .line_type = Plot_LineType_Solid
        , .xs = range_close
        , .ys = vector_to_iterator(&batch_close.value) 
        , .legenda = "Low price" 
        , .line_thickness = 2
        , .color = RGBA(.R=0x00, .G=0xFF, .B=0x20, .A=0xF0)};

    ScatterPlot_Settings settings = {
        .width = 1200
        , .height = 600
        , .x_label = "time"
        , .y_label = "price"
        , .title = "BITCOIN"
        , .vflip = false
        , .hflip = false
        , .padding_auto = true
        , .show_grid = true
        , .grid_color = RGBA_Gray
        , .serie_size = 2
        , .serie = (ScatterPlot_Series *[]) {&serie_open, &serie_close}
    };

    Plot image = scatter_plot_draw_from_settings(ALLOC(&alloc), &settings);

    /*
     * store plot as image
     */
    write_as_bmp(&image, "plot.bmp");
    plot_finalize(&image);

    finalize(ALLOC(&alloc));

    return true;
}


int main(void) {
	__show_candle_chart();

    printf("Programe exit..\n");
    return EXIT_SUCCESS;
}





