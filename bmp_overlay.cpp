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


u8 *convert_rgb_to_yuv420(u8 *rgb_data, u32 width, u32 height)
{
    u32 image_resolution = width * height;
    
    u32 yuv_data_size = image_resolution + (image_resolution / 2);
    u8 *yuv_data = (u8 *)malloc(yuv_data_size);

    u8 r, g, b;
    u32 pixel_row;

    u32 y_index = 0;
    u32 u_index = image_resolution;
    u32 v_index = image_resolution + (image_resolution / 4);

    
    // RGB data layout:
    // pixel rows are stored from bottom to top
    // pixel colors are stored in the BGR order
 
    for (u32 y = height; y > 0; y -= 2)
    {
        pixel_row = (y - 1) * width;
        
        for (u32 x = 0; x < width; x += 2)
        {   
            r = rgb_data[((x + pixel_row) * 3) + 2];
            g = rgb_data[((x + pixel_row) * 3) + 1];
            b = rgb_data[(x + pixel_row) * 3];
                        
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
            yuv_data[u_index++] = ((-38 * r - 74 * g + 112 * b + 128) >> 8) + 128;
            yuv_data[v_index++] = ((112 * r - 94 * g - 18 * b + 128) >> 8) + 128;


            r = rgb_data[((x + 1 + pixel_row) * 3) + 2];
            g = rgb_data[((x + 1 + pixel_row) * 3) + 1];
            b = rgb_data[(x + 1 + pixel_row) * 3];
                
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;               
        }
        
        pixel_row = (y - 2) * width;
        
        for (u32 x = 0; x < width; x++)
        {   
            r = rgb_data[((x + pixel_row) * 3) + 2];
            g = rgb_data[((x + pixel_row) * 3) + 1];
            b = rgb_data[(x + pixel_row) * 3];
                        
            yuv_data[y_index++] = ((66 * r + 129 * g + 25 * b + 128) >> 8) + 16;
        }
    }
    
    return yuv_data;
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

        u8 *yuv_data = convert_rgb_to_yuv420(rgb_data, bmp_header.width, bmp_header.height);
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
