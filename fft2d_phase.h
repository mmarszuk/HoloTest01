#pragma once

#include <stdexcept>

#include "ipp/latest/include/ippi.h"

#define FFT2D_PHASE_ASSERT_( __expr__ , __line__ ) if( ! (__expr__) ) throw std::runtime_error( #__expr__ "\nLine:" #__line__ "\nFile:" __FILE__ "\nFunction:" __FUNCTION__ )
#define FFT2D_PHASE_ASSERT( __expr__ ) FFT2D_PHASE_ASSERT_( __expr__ , __LINE__ )

class FFT2DPhase {
private:

    int imageWidth  = -1; // the previously width of image
    int imageHeight = -1; // the previously height of image
    Ipp32f *buffFlt0 = nullptr;
    Ipp32fc *buffC0  = nullptr;
    Ipp32fc *buffC1  = nullptr;
    IppiDFTSpec_C_32fc *pDFTSpec = nullptr;
    Ipp8u* pMemInit = nullptr;
    Ipp8u* pMemBuffer = nullptr;

    constexpr static double PI = 3.141592653589793238462643383279502884197169399375105820974944;
    constexpr static double EPSILON = 0.000001;

private:

    //The following method check there are need of the memory allocation
    bool isNeedRealloc(const int imageWidth, const int imageHeight);

    //The following method realloc all resource if need.
    void reAlloc(const int imageWidth, const int imageHeight);

    //The following method release all resource (memory).
    void allFree();

public:

    ~FFT2DPhase();

    void performFFTPhase(
        const Ipp8u imageInput[],  //the pixels of the image.
        const int imageWidth,      //the width of the image input and output.
        const int imageHeight,     //the height of the image.
        const int rowSize,         //the row size of the input image given in the bytes with padding.
        Ipp8u imageOutput[],       //the pixels of the output image imageWidth * imageHeight with zer padding
        const int percentTop,      //the percent of the top half of the image to the searching of the max amplitude.
        int roi                    //the real roi i equal ROI*2+1
    );


};

