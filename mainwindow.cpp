#include <stdexcept>

#include <QFileDialog>
#include <QMessageBox>
#include <QPicture>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ipp/latest/include/ipp.h"
#include "ipp/latest/include/ippi.h"
#include "ipp/latest/include/ipps.h"
#include "ipp/latest/include/ippvm.h"

#define XASSERT_( __expr__ , __line__ ) if( ! (__expr__) ) throw std::runtime_error( #__expr__ "\nLine:" #__line__ "\nFile:" __FILE__ "\nFunction:" __FUNCTION__ )
#define XASSERT( __expr__ ) XASSERT_( __expr__ , __LINE__ )

//Circle shift
template<class T>
void circShift(const T src[], T dst[], const int width, const int height, const int xshift, const int yshift)
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
void rectShift( const T src[], T dst[], const int width, const int height)
{
    circShift(src, dst, width, height, width/2, height/2 );
}

//Inversion rect shift
template<class T>
void invRectShift( const T src[], T dst[], const int width, const int height)
{
    circShift(src, dst, width, height, (width+1)/2, (height+1)/2 );
}


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


    //The following method compare two complex number by amplitude.
    template<typename T=Ipp32fc>
    inline static bool cmpAmplitude( const T *const a, const T *const b ) {
        return sqrt( a->re * a->re + a->im * a->im ) > sqrt( b->re * b->re + b->im * b->im );
    };

    //The following method find the pointer to the pixel with max aplitude in the top image.
    template<typename T=Ipp32fc>
    static T* findMaxAmplitude(
        T *const image,        //the image as complex numbers.
        const int topPercent,  //the percent of image height.
        const int imageWidth,  //the width of the image.
        const int imageHeight  //the height of the image.
    ) {
        XASSERT( imageHeight * topPercent / 100 > 0 );
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
        XASSERT( rowSize >= sizeof(T_FROM) * inputWidth );
        const int paddingSize = rowSize - sizeof(T_FROM) * inputWidth;
        T_FROM *const endInput = reinterpret_cast<T_FROM*>( reinterpret_cast<ONE_BYTE_TYPE*>(input) + inputHeight * rowSize );
        while( input < endInput ) {
            T_FROM *const endRow = input + inputWidth;
            while( input < endRow ) {
                *output++ = static_cast<T_TO>(*input++);
            }
            input = reinterpret_cast<T_FROM*>( reinterpret_cast<ONE_BYTE_TYPE*>(input) + paddingSize );
        }
    }

    //The following method convert the input to the output.
    template<typename T>
    static void findMinMax( T *const input, const int inputSize, T &min, T &max) {
        XASSERT( inputSize > 0 );
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

public:
    void performFFTPhase(
        Ipp8u imageData[],        //the pixels of the image.
        const int imageWidth,     //the width of the image.
        const int imageHeight,    //the height of the image.
        const int rowSize,        //the row size of the image given in the bytes with padding.
        const int percentTop,     //the percent of the top half of the image to the searching of the max amplitude.
        int roi                   //the real roi i equal ROI*2+1
    ) {
        const int imageSize = imageWidth * imageHeight;
        reAlloc(imageWidth,imageHeight);

        convert(imageData, buffFlt0, imageWidth, imageHeight, rowSize);

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
        XASSERT( idxMaxX + roi <  imageWidth );
        XASSERT( idxMaxX - roi >= 0 );
        XASSERT( idxMaxY + roi <  imageHeight );
        XASSERT( idxMaxY - roi >= 0 );

        for( int ys=idxMaxY-roi, yd=imageHeight/2-roi ; ys<=idxMaxY+roi  ; ys++, yd++ ) {
            for( int xs=idxMaxX-roi, xd=imageWidth/2-roi ; xs<=idxMaxX+roi  ; xs++, xd++ ) {
                buffC1[ yd * imageWidth + xd ] = buffC0[ ys * imageWidth + xs ];
            }
        }

        //bufOut = shift( fft( ishift( bufOut ) ) )
        invRectShift( buffC1, buffC0, imageWidth, imageHeight );
        ippiDFTInv_CToC_32fc_C1IR(buffC0, sizeof(Ipp32fc) * imageWidth, pDFTSpec, pMemBuffer);
        rectShift( buffC0, buffC1, imageWidth, imageHeight );


        // Phase FFT
        ippsPhase_32fc(buffC1, buffFlt0, imageSize);
        Ipp32f minPhase, maxPhase;
        findMinMax(buffFlt0,imageSize,minPhase,maxPhase);




        // Computes common logarithm of each vector element
        // Sorry for name pMagn, because both buffReIn and pMagn contain the phase!
        //ippsLog10_32f_A11(bufReIn, pMagn, w * h);

        // Histogram alignment
        //ippsMulC_32f_I(mulCVal, pMagn, w * h); // Contrast
        //ippsAddC_32f_I(addCVal, pMagn, w * h); // Brightness

        // Convert back to input buffer type
        ippsConvert_32f8u_Sfs(buffFlt0, imageData, imageSize, ippRndZero, 1);

        // Put calculated data to fft queue
        auto bufferOutput = std::make_unique<unsigned char[]>(bufferSize);
        memcpy(bufferOutput.get(), bufCam, bufferSize);
        if(fftQueue.push(bufferOutput.get()))
        {
            bufferOutput.release();
        }

    }


};

//void performFFTPhase(unsigned char *bufferData)
//{
//    try
//    {

//    }
//    catch (const std::exception &e)
//    {
//        qDebug() << e.what();
//    }
//}



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}


MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pB_Open_clicked()
{
    const QString imagePath = QFileDialog::getOpenFileName(this,"Open image",QString(),"Images (*.png *.xpm *.jpg)");
    if( imagePath.isNull() ) {
        return;
    }
    QImage tmpImage( imagePath );
    if( imagePath.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can not open file [%1].").arg(imagePath) );
        return;
    }
    image = tmpImage.convertToFormat(QImage::Format_Grayscale8);
    if( image.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can convert to the grayscale.").arg(imagePath) );
        return;
    }
    ui->lbl_Origin->setPixmap( QPixmap::fromImage(image) );
}



void MainWindow::on_pB_Exit_clicked()
{
    qApp->exit(0);
}


void MainWindow::on_pB_Compute_clicked()
{

}

