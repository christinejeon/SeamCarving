#include <stdio.h>
#include <math.h>
#include "seamcarving.h"
#include "c_img.h"

struct coordinate
{
    float x;
    float y;
};

void calc_energy(struct rgb_img *im, struct rgb_img **grad){
    /*Plan of attack:
     * Make a new image grad to store the data
     * Figure out how many pixels we're dealing with
     * Loop through the image and extract the grad value, taking care to deal with edge cases (
     */
    // are RGB stored in that order? Hopefully...
    //Each color is 8 bits, (single byte).
    //Set each color pixel in the storage image to the same value.
    create_img(grad, im->height, im->width);
    int pix_num = im->height * im->width;
    int i = 0;
    int col = 0;
    uint8_t value = 0;
    float temp_sum = 0;
    while(i < pix_num) {
        //exploit integer division rounding to get nicely indexed xs and ys.
        int y = i / (im->width);
        int x = i - y * (im->width);

        //declare all the coordinates we need
        struct coordinate up;
        struct coordinate down;
        struct coordinate left;
        struct coordinate right;

        //consider side edge cases
        //side edges
        if (x == 0) {
            right.x = x + 1;
            right.y = y;
            left.x = im->width - 1;
            left.y = y;
        } else if (x == im->width - 1) {
            right.x = 0;
            right.y = y;
            left.x = x - 1;
            left.y = y;
        } else {
            right.x = x + 1;
            right.y = y;
            left.x = x - 1;
            left.y = y;
        }


        if (y == 0) {
            up.x = x;
            up.y = im->height - 1;
            down.x = x;
            down.y = y + 1;
        } else if (y == im->height - 1) {
            up.x = x;
            up.y = y - 1;
            down.x = x;
            down.y = 0;
        } else {
            up.x = x;
            up.y = y - 1;
            down.x = x;
            down.y = y + 1;
        }

        //at this point we have our neighbour coordinates set.
        //Next step: use get pixel to get the col difference of each pixel
        //Nested loops


        while(col < 3){
            temp_sum = temp_sum + pow(get_pixel(im, left.y, left.x, col) - get_pixel(im, right.y, right.x, col), 2);
            col ++;
        }
        col = 0;

        while (col < 3){
            temp_sum =  temp_sum + pow(get_pixel(im, up.y, up.x, col) - get_pixel(im, down.y, down.x, col) , 2);
            col++;
        }

        col = 0;
        value = (sqrt(temp_sum))/10;
        //the following line is testing code, be sure to delete it.


        set_pixel(*grad, y, x, value, value, value);

        col = 0;
        i ++;
        value = 0;
        temp_sum = 0;

    }

}

void dynamic_seam(struct rgb_img *grad, double **best_arr){
    //build to arrays of length widthxheight. Use one to record which pixels you've calculated.

    int height = grad->height;
    int width = grad->width;
    //filled pixels is a placeholder array: if the corresponding pixel value is known, it's set = to 1.

    int pix_num = height*width;

    //malloc space for best_arr to exist in
    *best_arr = (double *)malloc(pix_num*sizeof(double));
    int i, x, y = 0;
    i = 0;
    //tester modification:

    //assign top row
    while(i<width){
        (*best_arr)[i] = get_pixel(grad, 0, i, 0);
        i ++;
    }


    while(i<pix_num){
        //handle left right edge cases first
        //left side edge cases:
        y = i/width;
        x = i - y*width;
        if(x % width == 0){
            (*best_arr)[i] = get_pixel(grad, y, x, 0) + fmin((*best_arr)[i - width], (*best_arr)[i - width + 1]);

        }
        //right edge cases:
        else if((x + 1)%width == 0){
            (*best_arr)[i] = get_pixel(grad, y, x, 0) + fmin((*best_arr)[i - width], (*best_arr)[i - width - 1]);
        }
        else{
            // since my min function only works on two, I'm putting a little mustard on dis
            (*best_arr)[i] = get_pixel(grad, y, x, 0) + fmin((*best_arr)[i - width + 1], fmin((*best_arr)[i - width], (*best_arr)[i - width - 1]));
        }
        i ++;
    }
}

//down to up
void recover_path(double *best, int height, int width, int **path)
{
    float min_val = 10000;
    int width_ind;
    int saved_w_ind;

    //find min in last row
    int last_ind = ((height - 1) * width) - 1;  //last ind of full rows
    *path = (int *)malloc((height)*sizeof(int));

    for (int i = 0; i < width; i++)
    {
        if (best[last_ind + 1 + i] < min_val)
        {
            min_val = best[last_ind + 1 + i];
            width_ind = i;
        }
    }
    (*path)[height - 1] = width_ind;

    for (int j = 1; j < height; j++) //j == row num
    {
        min_val = 10000;
        last_ind = ((height - j - 1) * width) - 1; //last ind of full rows

        //edge cases
        if (width_ind == 0) //leftmost
        {
            for (int i = width_ind + 1; i < (width_ind + 3); i++) //below, bottom right
            {
                if (best[last_ind + i] < min_val)
                {
                    min_val = best[last_ind + i];
                    saved_w_ind = i - 1;
                }
            }
        }
        else if (width_ind == width - 1) //rightmost
        {
            for (int i = width_ind; i < (width_ind + 2); i++) //bottom left, below
            {
                if (best[last_ind + i] < min_val)
                {
                    min_val = best[last_ind + i];
                    saved_w_ind = i - 1;
                }
            }
        }
        else
        {

            for (int i = width_ind; i < (width_ind + 3); i++)
            {
                if (best[last_ind + i] < min_val)
                {
                    min_val = best[last_ind + i];
                    saved_w_ind = i - 1;
                }
            }
        }
        width_ind = saved_w_ind;

        (*path)[height - j - 1] = width_ind;
    }
}

void remove_seam(struct rgb_img *src, struct rgb_img **dest, int *path){
    //create an image in dest of heightxwidth-1
    //call set_pixel in a loop to fill the new image.
    create_img(dest, src->height, src->width - 1);
    for(int y = 0; y<src->height; y++){
        for(int x = 0; x<src->width ; x ++){
            if(x < path[y]){
                int red = get_pixel(src, y, x, 0);
                int green = get_pixel(src, y, x, 1);
                int blue = get_pixel(src, y, x, 2);
                set_pixel(*dest, y, x, red, green, blue);
            }
            if(x > path[y]){
                int red = get_pixel(src, y, x, 0);
                int green = get_pixel(src, y, x, 1);
                int blue = get_pixel(src, y, x, 2);
                set_pixel(*dest, y, x -1, red, green, blue);
            }
        }
    }
}
