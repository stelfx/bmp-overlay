#include "bmp_overlay.h"


u8 *read_bitmap_data(char *filename, bitmap_header *bmp_header)
{
    FILE *fin;
    fin = fopen(filename, "rb");
    if (fin)
    {
        fread(bmp_header, sizeof(bitmap_header), 1, fin);
        
        if (bmp_header->file_type == 0x4D42)
        {
            if (bmp_header->compression != 0)
            {
                fprintf(stderr, "error: the BMP file should be uncompressed\n");
                exit(1);
            }
            
            u32 rgb_data_size = bmp_header->height * bmp_header->width * 3;
            u8 *rgb_data = (u8 *)malloc(rgb_data_size);
                
            fseek(fin, bmp_header->bitmap_offset, SEEK_SET);
            fread(rgb_data, rgb_data_size, 1, fin);         
            fclose(fin);
            
            return rgb_data;            
        }
        else
        {
            fprintf(stderr, "error: file %s is not a BMP file\n", filename);
            exit(1);            
        }
    }
    else
    {        
        fprintf(stderr, "error: unable to read %s\n", filename);
        exit(1);        
    }    
}


void convert_rgb_to_yuv420(u8 *rgb_data, u8 *yuv_data, u32 width, u32 y_index, u32 u_index, u32 v_index,
                          u32 start_row, u32 end_row)
{
    u8 r, g, b;
    u32 pixel_row;
    
    // RGB data layout:
    // pixel rows are stored from bottom to top
    // pixel colors are stored in the BGR order

    for (u32 y = start_row; y > end_row; y -= 2)
    {
        pixel_row = (y - 1) * width;
        
        for (u32 x = 0; x < width; x += 2)
        {   
            r = rgb_data[((x + pixel_row) * 3) + 2];
            g = rgb_data[((x + pixel_row) * 3) + 1];
            b = rgb_data[(x + pixel_row) * 3];
                        
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8);
            yuv_data[u_index++] = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            yuv_data[v_index++] = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;


            r = rgb_data[((x + 1 + pixel_row) * 3) + 2];
            g = rgb_data[((x + 1 + pixel_row) * 3) + 1];
            b = rgb_data[(x + 1 + pixel_row) * 3];
                
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8);               
        }
        
        pixel_row = (y - 2) * width;
        
        for (u32 x = 0; x < width; x++)
        {   
            r = rgb_data[((x + pixel_row) * 3) + 2];
            g = rgb_data[((x + pixel_row) * 3) + 1];
            b = rgb_data[(x + pixel_row) * 3];
                        
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8);
        }
    }
}


void convert_rgb_to_yuv420_ssse3(u8 *rgb_data, u8 *yuv_data, u32 width, u32 y_index, u32 u_index, u32 v_index,
                                u32 start_row, u32 end_row)
{

    u8 r, g, b;
    u32 simd_aligned_width = width & (~15);
    
    u32 pixel_row_1, pixel_row_2;
    u32 x;

    lane_v3 rgb_shuffle_lo = set_epi8_lo(0, 3, 6, 9);
    lane_v3 rgb_shuffle_hi = set_epi8_hi(0, 3, 6, 9);
    
    m128i y_shuffle_lo = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        15,   13,   11,   9,
        7,    5,    3,    1);
    m128i y_shuffle_hi = _mm_set_epi8(
        15,   13,   11,   9,
        7,    5,    3,    1,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff);

    m128i uv_shuffle_lo_1 = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        12,   8,    4,    0);
    m128i uv_shuffle_lo_2 = _mm_set_epi8(
        0xff, 0xff, 0xff, 0xff,
        0xff, 0xff, 0xff, 0xff,
        12,   8,    4,    0,
        0xff, 0xff, 0xff, 0xff);
    
    lane_v3 y_coeff = set1_epi16(25, 129, 66);
    lane_v3 u_coeff = set1_epi16(112, -74, -38);
    lane_v3 v_coeff = set1_epi16(-18, -94, 112);

    m128i uv_const = _mm_set1_epi32(128);

    // RGB data layout:
    // pixel rows are stored from bottom to top
    // pixel colors are stored in the BGR order
     
    for (u32 row = start_row; row > end_row; row -= 2)
    {
        pixel_row_1 = (row - 1) * width;
        pixel_row_2 = (row - 2) * width;
        
        for (x = 0; x < simd_aligned_width; x += 16)
        {
            m128i rgb_lane_1 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_1) * 3]);
            m128i rgb_lane_2 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_1) * 3 + 12]);
            m128i rgb_lane_3 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_1) * 3 + 24]);
            m128i rgb_lane_4 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_1) * 3 + 36]);
           
            m128i rgb_lane_5 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_2) * 3]);
            m128i rgb_lane_6 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_2) * 3 + 12]);
            m128i rgb_lane_7 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_2) * 3 + 24]);
            m128i rgb_lane_8 = _mm_lddqu_si128((m128i* )&rgb_data[(x + pixel_row_2) * 3 + 36]);


            m128i red_lane_1 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_1, rgb_shuffle_lo.r), _mm_shuffle_epi8(rgb_lane_2, rgb_shuffle_hi.r));
            m128i green_lane_1 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_1, rgb_shuffle_lo.g), _mm_shuffle_epi8(rgb_lane_2, rgb_shuffle_hi.g));
            m128i blue_lane_1 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_1, rgb_shuffle_lo.b), _mm_shuffle_epi8(rgb_lane_2, rgb_shuffle_hi.b));
            
            m128i red_lane_2 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_3, rgb_shuffle_lo.r), _mm_shuffle_epi8(rgb_lane_4, rgb_shuffle_hi.r));
            m128i green_lane_2 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_3, rgb_shuffle_lo.g), _mm_shuffle_epi8(rgb_lane_4, rgb_shuffle_hi.g));
            m128i blue_lane_2 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_3, rgb_shuffle_lo.b), _mm_shuffle_epi8(rgb_lane_4, rgb_shuffle_hi.b));
            
            m128i red_lane_3 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_5, rgb_shuffle_lo.r), _mm_shuffle_epi8(rgb_lane_6, rgb_shuffle_hi.r));
            m128i green_lane_3 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_5, rgb_shuffle_lo.g), _mm_shuffle_epi8(rgb_lane_6, rgb_shuffle_hi.g));
            m128i blue_lane_3 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_5, rgb_shuffle_lo.b), _mm_shuffle_epi8(rgb_lane_6, rgb_shuffle_hi.b));
            
            m128i red_lane_4 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_7, rgb_shuffle_lo.r), _mm_shuffle_epi8(rgb_lane_8, rgb_shuffle_hi.r));
            m128i green_lane_4 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_7, rgb_shuffle_lo.g), _mm_shuffle_epi8(rgb_lane_8, rgb_shuffle_hi.g));
            m128i blue_lane_4 = _mm_or_si128(_mm_shuffle_epi8(rgb_lane_7, rgb_shuffle_lo.b), _mm_shuffle_epi8(rgb_lane_8, rgb_shuffle_hi.b));
            

            m128i y_lane_1 = _mm_add_epi16(_mm_mullo_epi16(red_lane_1, y_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_1, y_coeff.g), _mm_mullo_epi16(blue_lane_1, y_coeff.b)));
            m128i y_lane_2 = _mm_add_epi16(_mm_mullo_epi16(red_lane_2, y_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_2, y_coeff.g), _mm_mullo_epi16(blue_lane_2, y_coeff.b)));

            m128i u_lane_1 = _mm_add_epi16(_mm_srai_epi32(_mm_add_epi16(_mm_mullo_epi16(red_lane_1, u_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_1, u_coeff.g), _mm_mullo_epi16(blue_lane_1, u_coeff.b))), 8), uv_const);
            m128i u_lane_2 = _mm_add_epi16(_mm_srai_epi32(_mm_add_epi16(_mm_mullo_epi16(red_lane_2, u_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_2, u_coeff.g), _mm_mullo_epi16(blue_lane_2, u_coeff.b))), 8), uv_const);

            m128i v_lane_1 = _mm_add_epi16(_mm_srai_epi32(_mm_add_epi16(_mm_mullo_epi16(red_lane_1, v_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_1, v_coeff.g), _mm_mullo_epi16(blue_lane_1, v_coeff.b))), 8), uv_const);
            m128i v_lane_2 = _mm_add_epi16(_mm_srai_epi32(_mm_add_epi16(_mm_mullo_epi16(red_lane_2, v_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_2, v_coeff.g), _mm_mullo_epi16(blue_lane_2, v_coeff.b))), 8), uv_const);

            y_lane_1 = _mm_shuffle_epi8(y_lane_1, y_shuffle_lo);
            y_lane_2 = _mm_shuffle_epi8(y_lane_2, y_shuffle_hi);
            
            u_lane_1 = _mm_shuffle_epi8(u_lane_1, uv_shuffle_lo_1);
            u_lane_2 = _mm_shuffle_epi8(u_lane_2, uv_shuffle_lo_2);
            
            v_lane_1 = _mm_shuffle_epi8(v_lane_1, uv_shuffle_lo_1);
            v_lane_2 = _mm_shuffle_epi8(v_lane_2, uv_shuffle_lo_2);

            _mm_storeu_si128((m128i* )&yuv_data[y_index], _mm_or_si128(y_lane_1, y_lane_2));
            _mm_storel_epi64((m128i* )&yuv_data[u_index], _mm_or_si128(u_lane_1, u_lane_2));
            _mm_storel_epi64((m128i* )&yuv_data[v_index], _mm_or_si128(v_lane_1, v_lane_2));
         
                                                       
            m128i y_lane_3 = _mm_add_epi16(_mm_mullo_epi16(red_lane_3, y_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_3, y_coeff.g), _mm_mullo_epi16(blue_lane_3, y_coeff.b)));
            m128i y_lane_4 = _mm_add_epi16(_mm_mullo_epi16(red_lane_4, y_coeff.r), _mm_add_epi16(_mm_mullo_epi16(green_lane_4, y_coeff.g), _mm_mullo_epi16(blue_lane_4, y_coeff.b)));

            y_lane_3 = _mm_shuffle_epi8(y_lane_3, y_shuffle_lo);
            y_lane_4 = _mm_shuffle_epi8(y_lane_4, y_shuffle_hi);

            _mm_storeu_si128((m128i* )&yuv_data[y_index + width], _mm_or_si128(y_lane_3, y_lane_4));

            
            y_index += 16;
            u_index += 8;
            v_index += 8;
        }

        // handle leftovers 
        if (simd_aligned_width != width)
        {
            for (; x < width; x += 2)
            {   
                r = rgb_data[((x + pixel_row_1) * 3) + 2];
                g = rgb_data[((x + pixel_row_1) * 3) + 1];
                b = rgb_data[(x + pixel_row_1) * 3];
                        
                yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b) >> 8); 
                yuv_data[u_index++] = ((-38 * r - 74 * g + 112 * b) >> 8) + 128;
                yuv_data[v_index++] = ((112 * r - 94 * g - 18 * b) >> 8) + 128;


                r = rgb_data[((x + 1 + pixel_row_1) * 3) + 2];
                g = rgb_data[((x + 1 + pixel_row_1) * 3) + 1];
                b = rgb_data[(x + 1 + pixel_row_1) * 3];
                        
                yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b) >> 8);
            }
        
            for (x = simd_aligned_width; x < width; x++)
            {   
                r = rgb_data[((x + pixel_row_2) * 3) + 2];
                g = rgb_data[((x + pixel_row_2) * 3) + 1];
                b = rgb_data[(x + pixel_row_2) * 3];
                        
                yuv_data[y_index + x] = ((66 * r + 129 * g + 25 * b) >> 8);
            }
        }

        y_index += width;
    }

}


void create_threads(u8 *rgb_data, u8 *yuv_data, u32 width, u32 height)
{
    u32 image_resolution = width * height;
    
    u32 y_index = 0;
    u32 u_index = image_resolution;
    u32 v_index = image_resolution + (image_resolution / 4);

    u32 thread_count = 4;
    u32 height_partition = ((u32)floor(height / thread_count)) & (~1);
    
    u32 start_row = height;
    u32 end_row = height - height_partition;

    std::vector<std::thread> threads(thread_count - 1);
    for (u32 i = 0; i < thread_count - 1; i++)
    {
        threads[i] = std::thread(convert_rgb_to_yuv420_ssse3, rgb_data, yuv_data, width, y_index,
                                      u_index, v_index, start_row, end_row);
        y_index += width * height_partition;
        u_index += (width * height_partition) / 4;
        v_index += (width * height_partition) / 4;
        start_row -= height_partition;
        end_row -= height_partition;
    }

    convert_rgb_to_yuv420_ssse3(rgb_data, yuv_data, width, y_index, u_index, v_index, start_row, 0);
        
    for (u32 i = 0; i < thread_count - 1; i++)
    {
        threads[i].join();
    }
}


void overlay_image_on_video(u8 *image_data, u32 image_width, u32 image_height, char *video_filename,
                            u32 video_frame_width, u32 video_frame_height, char *output_filename)
{
    u32 video_resolution = video_frame_width * video_frame_height;
    u32 video_frame_size = video_resolution + (video_resolution / 2);

    u8 *video_data_ptr; 
    u8 *image_data_ptr;

    u32 image_resolution = image_width * image_height;
    
    u32 x_offset = 0;
    
    FILE *fin = fopen(video_filename, "rb");

    if (fin)
    {
        fseek(fin,0,SEEK_END);
        u32 video_data_size = ftell(fin);
        rewind(fin);

        u8 *video_data = (u8 *)malloc(video_data_size);
        
        fread(video_data, video_data_size, 1, fin);
        fclose(fin);
       
        u32 number_of_frames = video_data_size / video_frame_size;  
    
        for (u32 f = 0; f < number_of_frames; f++)
        {
            // overlay Y component
            video_data_ptr = (f * video_frame_size) + video_data + video_frame_width + x_offset;
            image_data_ptr = image_data;
            
            for (u32 y = 0; y < image_height; y++) {
                memcpy(video_data_ptr, image_data_ptr, image_width);
                video_data_ptr += video_frame_width;
                image_data_ptr += image_width;
            }

            // overlay U component
            video_data_ptr = (f * video_frame_size) + video_data + video_resolution + (video_frame_width / 2)
                + (x_offset / 2);
            image_data_ptr = image_data + image_resolution;
            
            for (u32 u = 0; u < image_height / 2; u++) {
                memcpy(video_data_ptr, image_data_ptr, image_width / 2);
                video_data_ptr += video_frame_width / 2;
                image_data_ptr += image_width / 2;
            }

            // overlay V component
            video_data_ptr = (f * video_frame_size) + video_data + video_resolution + (video_resolution / 4)
                + (video_frame_width / 2) + (x_offset / 2);
            image_data_ptr = image_data + image_resolution + (image_resolution / 4);
            
            for (u32 v = 0; v < image_height / 2; v++) {
                memcpy(video_data_ptr, image_data_ptr, image_width / 2);
                video_data_ptr += video_frame_width / 2;
                image_data_ptr += image_width / 2;
            }
        
            x_offset++;
        }
 
        FILE *fout = fopen(output_filename, "wb");
        if (fout) {
            fwrite (video_data, 1, video_data_size, fout);
            fclose (fout);
            free(video_data);
        }
        else
        {
            fprintf(stderr, "error: unable to write to %s\n", output_filename);
            exit(1);
        }
        
    }
    else
    {        
        fprintf(stderr, "error: unable to read %s\n", video_filename);
        exit(1);        
    }     
}


int main(int arg_count, char **args)
{    
    if(arg_count == 6)
    {        
        char *image_filename = args[1];
        char *video_filename = args[2];
        char *output_filename = args[3];
        u32 video_frame_width = atoi(args[4]);
        u32 video_frame_height = atoi(args[5]);

        bitmap_header bmp_header;        
        u8 *rgb_data = read_bitmap_data(image_filename, &bmp_header);

        if (bmp_header.width % 2 != 0)
        {
            fprintf(stderr, "error: the image width should be a multiple of 2\n");
            exit(1);
        }
        else if (bmp_header.height % 2 != 0)
        {
            fprintf(stderr, "error: the image height should be a multiple of 2\n");
            exit(1);
        }

        
        if (bmp_header.width > video_frame_width)
        {
            fprintf(stderr, "error: the image width should not exceed the width of a video frame\n");
            exit(1);
        }
        else if (bmp_header.height > video_frame_height)
        {
            fprintf(stderr, "error: the image height should not exceed the height of a video frame\n");
            exit(1);
        }        

        u32 image_resolution = bmp_header.width * bmp_header.height;
    
        u32 yuv_data_size = image_resolution + (image_resolution / 2);
        u8 *yuv_data = (u8 *)malloc(yuv_data_size);

        create_threads(rgb_data, yuv_data, bmp_header.width, bmp_header.height);        
        free(rgb_data);
                
        overlay_image_on_video(yuv_data, bmp_header.width, bmp_header.height, video_filename, video_frame_width, video_frame_height, output_filename);
        free(yuv_data);
    }
    else
    {
        fprintf(stderr, "\nUsage: %s -i -v -o -w -h\n\n", args[0]);
        printf("-i: BMP image file name\n");
        printf("-v: input YUV420 video file name\n");
        printf("-o: output YUV420 video file name\n");
        printf("-w: video frame width\n");
        printf("-h: video frame height\n");
    }
    
    return 0;
}
