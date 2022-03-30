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

private:

    //The following method check there are need of the memory allocation
    bool isNeedRealloc(const int imageWidth, const int imageHeight);

    //The following method realloc all resource if need.
    void reAlloc(const int imageWidth, const int imageHeight);

    //The following method release one variable and set it on the zero value.
    template<typename T>
    static void oneFree( T **memory ) {
        if( *memory != nullptr ) {
            delete[] *memory;
            *memory = nullptr;
        }
    }

    //The following method release one variable and set it on the zero value.
    template<typename T>
     static void ippOneFree( T **memory ) {
        if( *memory != nullptr ) {
            ippFree( *memory );
            *memory = nullptr;
        }
    }

    //The following method release all resource (memory).
    void allFree();

    //Circle shift
    template<class T>
    static void circShift(const T src[], T dst[], const int width, const int height, const int xshift, const int yshift)
    {
      for (int y = 0, yy = yshift; y < height; y++,yy++) {
        if( yy >= height ) yy = 0;
        for (int x = 0, xx=xshift; x < width; x++,xx++) {
          if( xx >= width ) xx = 0;
          dst[yy * width + xx] = src[y * width + x];
        }
      }
    }

    //Rect shift
    template<class T>
    inline static void rectShift( const T src[], T dst[], const int width, const int height)
    {
        circShift(src, dst, width, height, width/2, height/2 );
    }

    //Inversion rect shift
    template<class T>
    inline static void invRectShift( const T src[], T dst[], const int width, const int height)
    {
        circShift(src, dst, width, height, (width+1)/2, (height+1)/2 );
    }

    //The following method compute the amplitude of the complex number
    template<typename T=Ipp32fc>
    inline static auto amplitude( const T *const a) {
        return sqrt( a->re * a->re + a->im * a->im );
    };

    //The following method compare two complex number by amplitude.
    template<typename T=Ipp32fc>
    inline static bool cmpAmplitude( const T *const a, const T *const b ) {
        return amplitude(a) > amplitude(b);
    };

    //The following method find the pointer to the pixel with max aplitude in the top image.
    template<typename T>
    static const T* findMaxAmplitude(
        const T *image,        //the image as complex numbers.
        const int topPercent,  //the percent of image height.
        const int imageWidth,  //the width of the image.
        const int imageHeight  //the height of the image.
    ) {
        FFT2D_PHASE_ASSERT( imageHeight * topPercent / 100 > 0 );
        const T *const end = image + imageWidth * (imageHeight * topPercent / 100);
        const T *max = image++;
        while( image < end ) {                   // from begin to the end of the image top.
            if( cmpAmplitude(image,max) ) {
                max = image;
            }
            image ++ ;
        }
        return max;
    }

    //The following method convert the input to the output.
    template<typename T_FROM, typename T_TO>
    static void convert(
        const T_FROM *input,           //pointer to the input buffer with padding.
        T_TO *output,                  //pointer to the out buffer.
        const int inputWidth,          //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const int inputHeight,         //the height of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const int rowSize              //the row size of the buffer given in the bytes with padding.
    ) {
        using ONE_BYTE_TYPE = uint8_t;
        static_assert( sizeof(ONE_BYTE_TYPE) == 1, "sizeof(ONE_BYTE_TYPE) == 1" );
        FFT2D_PHASE_ASSERT( rowSize >= sizeof(T_FROM) * inputWidth );
        const int paddingSize = rowSize - sizeof(T_FROM) * inputWidth;
        const T_FROM *const endInput = reinterpret_cast<const T_FROM*>( reinterpret_cast<const ONE_BYTE_TYPE*>(input) + inputHeight * rowSize );
        while( input < endInput ) {
            const T_FROM *const endRow = input + inputWidth;
            while( input < endRow ) {
                *output++ = static_cast<T_TO>( *input++ );
            }
            input = reinterpret_cast<const T_FROM*>( reinterpret_cast<const ONE_BYTE_TYPE*>(input) + paddingSize );
        }
    }

    //The following method convert the input to the output with scaling.
    template<typename T_FROM, typename T_TO>
    static void convertWithScale(
        const T_FROM *input,           //pointer to the input buffer with padding.
        T_TO *output,                  //pointer to the out buffer.
        const int size,                //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const T_FROM add,              //the value to the add
        const T_FROM mul               //the value to the mul
    ) {
        const T_FROM *const endInput = input + size;
        while( input < endInput ) {
            *output++ = static_cast<T_TO>( (*input + add) * mul );
            input ++ ;
        }
    }

    //The following method computing the min and max value.
    template<typename T>
    static void findMinMax( const T *input, const int inputSize, T &min, T &max) {
        FFT2D_PHASE_ASSERT( inputSize > 0 );
        const T *const end = input + inputSize;
        min = max = *input;
        while( ++input < end ) {
            if( min > *input ) {
                min = *input;
            } else if( max < *input ) {
                max = *input;
            }
        }
    }

    //The following method copy the square region from input to the center of output.
    template<typename T>
    static void copySquareToCenter(
        const T input[],
        T output[],
        const int imageWidth,          //the width of the input buffer given as number of elements.
        const int imageHeight,         //the height of the input buffer given as number of elements.
        const int inputX,              //the center X input image
        const int inputY,              //the center Y input image
        const int size                 //the side of the square
    ) {
        for( int ys=inputY-size, yd=imageHeight/2-size ; ys<=inputY+size  ; ys++, yd++ ) {
            for( int xs=inputX-size, xd=imageWidth/2-size ; xs<=inputX+size  ; xs++, xd++ ) {
                output[ ys * imageWidth + xs ] = input[ ys * imageWidth + xs ];
            }
        }
    }

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


    void test(
        const Ipp8u imageInput[],  //the pixels of the image.
        const int imageWidth,      //the width of the image input and output.
        const int imageHeight,     //the height of the image.
        const int rowSize,         //the row size of the input image given in the bytes with padding.
        Ipp8u imageOutput[],       //the pixels of the output image imageWidth * imageHeight with zer padding
        const int percentTop,      //the percent of the top half of the image to the searching of the max amplitude.
        int roi                    //the real roi i equal ROI*2+1
    );

};

