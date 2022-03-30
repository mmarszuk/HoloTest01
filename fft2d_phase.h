#pragma once


#define FFT2D_PHASE_ASSERT_( __expr__ , __line__ ) if( ! (__expr__) ) throw std::runtime_error( #__expr__ "\nLine:" #__line__ "\nFile:" __FILE__ "\nFunction:" __FUNCTION__ )
#define FFT23_PHASE_ASSERT( __expr__ ) FFT2D_PHASE_ASSERT_( __expr__ , __LINE__ )


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

    ~FFT2DPhase() {
        allFree();
    }

    //The following method check there are need of the memory allocation
    bool isNeedRealloc(const int imageWidth, const int imageHeight) {
        return imageWidth != this->imageWidth || imageHeight != this->imageHeight;
    }

    //The following method realloc all resource if need.
    void reAlloc(const int imageWidth, const int imageHeight) {
        if( ! isNeedRealloc(imageWidth,imageHeight) ) {
            return;
        }
        this->imageWidth  = imageWidth;
        this->imageHeight = imageHeight;
        allFree();
        buffFlt0 = new Ipp32f[imageWidth * imageHeight];
        buffC0   = new Ipp32fc[imageWidth * imageHeight];
        buffC1   = new Ipp32fc[imageWidth * imageHeight];

        {
            const IppiSize roiSize = {imageWidth, imageHeight};
            int pSizeSpec, pSizeInit, pSizeBuf;
            ippiDFTGetSize_C_32fc(roiSize, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, &pSizeSpec, &pSizeInit, &pSizeBuf);
            pDFTSpec = (IppiDFTSpec_C_32fc*) ippMalloc(pSizeSpec);
            if(pSizeInit > 0) {
                pMemInit = (Ipp8u*) ippMalloc(pSizeInit);
            }
            if(pSizeBuf > 0) {
                pMemBuffer = (Ipp8u*) ippMalloc(pSizeBuf);
            }
            ippiDFTInit_C_32fc(roiSize, IPP_FFT_DIV_INV_BY_N, ippAlgHintNone, pDFTSpec, pMemInit);
        }
    }

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
    void allFree() {
        oneFree( &buffFlt0 );
        oneFree( &buffC0 );
        oneFree( &buffC1 );
        ippOneFree( &pDFTSpec );
        ippOneFree( &pMemInit );
        ippOneFree( &pMemBuffer );
    }

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
    inline static T amplitude( const T *const a) {
        return sqrt( a->re * a->re + a->im * a->im );
    };

    //The following method compare two complex number by amplitude.
    template<typename T=Ipp32fc>
    inline static bool cmpAmplitude( const T *const a, const T *const b ) {
        return amplitude(a) > amplitude(b);
    };

    //The following method find the pointer to the pixel with max aplitude in the top image.
    template<typename T=Ipp32fc>
    static T* findMaxAmplitude(
        T *const image,        //the image as complex numbers.
        const int topPercent,  //the percent of image height.
        const int imageWidth,  //the width of the image.
        const int imageHeight  //the height of the image.
    ) {
        FFT23_PHASE_ASSERT( imageHeight * topPercent / 100 > 0 );
        const T *const end = image + imageWidth * (imageHeight * topPercent / 100);
        T *const max = image++;
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
        T_FROM *const input,           //pointer to the input buffer with padding.
        T_TO *output,                  //pointer to the out buffer.
        const int inputWidth,          //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const int inputHeight,         //the height of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const int rowSize              //the row size of the buffer given in the bytes with padding.
    ) {
        using ONE_BYTE_TYPE = uint8_t;
        static_assert( sizeof(ONE_BYTE_TYPE) == 1, "sizeof(ONE_BYTE_TYPE) == 1" );
        FFT23_PHASE_ASSERT( rowSize >= sizeof(T_FROM) * inputWidth );
        const int paddingSize = rowSize - sizeof(T_FROM) * inputWidth;
        const T_FROM *const endInput = reinterpret_cast<T_FROM*>( reinterpret_cast<ONE_BYTE_TYPE*>(input) + inputHeight * rowSize );
        while( input < endInput ) {
            T_FROM *const endRow = input + inputWidth;
            while( input < endRow ) {
                *output++ = static_cast<T_TO>(*input++);
            }
            input = reinterpret_cast<T_FROM*>( reinterpret_cast<ONE_BYTE_TYPE*>(input) + paddingSize );
        }
    }

    //The following method convert the input to the output with scaling.
    template<typename T_FROM, typename T_TO>
    static void convertWithScale(
        T_FROM *const input,           //pointer to the input buffer with padding.
        T_TO *output,                  //pointer to the out buffer.
        const int size,                //the width of the input buffer given as number of elements (e.g. all pixels from row without padding).
        const T_FROM add,              //the value to the add
        const T_FROM mul               //the value to the mul
    ) {
        const T_FROM *const endInput = input + size;
        while( input < endInput ) {
            *output = static_cast<T_TO>( (*input + add) * mul );
            input ++ ;
        }
    }

    //The following method computing the min and max value.
    template<typename T>
    static void findMinMax( T *const input, const int inputSize, T &min, T &max) {
        FFT23_PHASE_ASSERT( inputSize > 0 );
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
        for( int ys=inputX-size, yd=imageHeight/2-size ; ys<=inputX+size  ; ys++, yd++ ) {
            for( int xs=inputY-size, xd=imageWidth/2-size ; xs<=inputY+size  ; xs++, xd++ ) {
                output[ yd * imageWidth + xd ] = input[ ys * imageWidth + xs ];
            }
        }
    }



public:
    void performFFTPhase(
        const Ipp8u imageInput[],  //the pixels of the image.
        const int imageWidth,      //the width of the image input and output.
        const int imageHeight,     //the height of the image.
        const int rowSize,         //the row size of the input image given in the bytes with padding.
        const Ipp8u imageOutput[], //the pixels of the output image imageWidth * imageHeight with zer padding
        const int percentTop,      //the percent of the top half of the image to the searching of the max amplitude.
        int roi                    //the real roi i equal ROI*2+1
    ) {
        const int imageSize = imageWidth * imageHeight;
        reAlloc(imageWidth,imageHeight);

        convert(imageInput, buffFlt0, imageWidth, imageHeight, rowSize);

        // Returns a complex vector pDst constructed from real parts of the input vector buffFlt0.
        ippsRealToCplx_32f(buffFlt0, nullptr, buffC0, imageSize );

        //buffC0 = shift( fft( ishift( buffC0 ) ) )
        invRectShift( buffC0, buffC1, imageWidth, imageHeight );
        ippiDFTFwd_CToC_32fc_C1IR(buffC1, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
        rectShift( buffC1, buffC0, imageWidth, imageHeight );

        const Ipp32fc *const maxAmplitude = findMaxAmplitude(buffC0,percentTop/2,imageWidth,imageHeight);
        const int idxMaxX = (maxAmplitude - buffC0) % imageWidth;
        const int idxMaxY = (maxAmplitude - buffC0) / imageHeight;

        if( roi > idxMaxX              ) roi = idxMaxX;              //too near left?
        if( roi > idxMaxY              ) roi = idxMaxY;              //too near top?
        if( roi > imageWidth - idxMaxX ) roi = imageWidth - idxMaxX; //too near right?
        //never too near bottom, because we cutting out from TOP_PERCENT.

        memset( buffC1 , 0 , sizeof(buffC1[0]) * imageSize );
        FFT23_PHASE_ASSERT( idxMaxX + roi <  imageWidth );
        FFT23_PHASE_ASSERT( idxMaxX - roi >= 0 );
        FFT23_PHASE_ASSERT( idxMaxY + roi <  imageHeight );
        FFT23_PHASE_ASSERT( idxMaxY - roi >= 0 );

        copySquareToCenter( buffC0, buffC1, imageWidth, imageHeight, idxMaxX, idxMaxY, roi );

        //buffC1 = shift( fft( ishift( buffC1 ) ) )
        invRectShift( buffC1, buffC0, imageWidth, imageHeight );
        ippiDFTInv_CToC_32fc_C1IR(buffC0, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
        rectShift( buffC0, buffC1, imageWidth, imageHeight );

        // Phase FFT
        ippsPhase_32fc(buffC1, buffFlt0, imageSize);
        Ipp32f minPhase, maxPhase;
        findMinMax(buffFlt0,imageSize,minPhase,maxPhase);
        FFT23_PHASE_ASSERT( minPhase <= PI );
        FFT23_PHASE_ASSERT( maxPhase >= PI );

        const Ipp32f add = -minPhase;
        const Ipp32f range = maxPhase - minPhase; //do not div by zero.
        const Ipp32f mul = range < 0.0001 ? 1 : std::numeric_limits<Ipp8u>::max() / range;

        convertWithScale(buffFlt0,imageOutput,imageSize,add,mul);
    }

};

