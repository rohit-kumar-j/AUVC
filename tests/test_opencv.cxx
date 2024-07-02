#include <stdio.h>
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>
using namespace cv;

#include <iostream>
#include <vector>

const char* file_path2 = "/home/rohit/Github/AUVC/tests/test_opencv.cxx";

int main()
{
    printf("\n-------------------\ntest\n");
    // We need an input image. (can be grayscale or color)
    // if (argc < 2)
    // {
    //     cerr << "We need an image to process here. Please run: colorMap [path_to_image]" << endl;
    //     return -1;
    // }
    const char* file_path = "/home/rohit/Github/AUVC/data/test.png";
    const char* file_path2 = "/home/rohit/Github/AUVC/tests/new.png";
    Mat img_in = imread(file_path);
    if(img_in.empty())
    {
        printf("Sample image (%s) is empty. Please adjust your path, so it points to a valid input image!",file_path);
        return -1;
    }
    // Holds the colormap version of the image:
    Mat img_color;
    // Apply the colormap:
    applyColorMap(img_in, img_color, COLORMAP_JET);
    // Show the result:
    cv::imshow("colorMap", img_color);

    std::vector<int> compression_params;
    compression_params.push_back(IMWRITE_PNG_COMPRESSION);
    printf("what is IMWRITE_PNG_COMPRESSION %d\n",IMWRITE_PNG_COMPRESSION);
    compression_params.push_back(1);

     bool result = false;
    try
    {
        result = imwrite(file_path2, img_color, compression_params);
    }
    catch (const cv::Exception& ex)
    {
        fprintf(stderr, "Exception converting image to PNG format: %s\n", ex.what());
    }

    if (result)
        printf("Saved PNG file with alpha data.\n");
    else
        printf("ERROR: Can't save PNG file.\n");

    // cv::imwrite(file_path2,img_color,)
    printf("Does the file (%s) read?... %zu\n",file_path,cv::imcount(file_path));
    // waitKey(0);
    return 0;
}
