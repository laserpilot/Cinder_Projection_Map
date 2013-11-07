#include "cinder/app/AppNative.h"
#include "cinder/gl/gl.h"
#include "cinder/Surface.h"
#include "cinder/gl/Texture.h"
#include "cinder/qtime/QuickTime.h"
#include "cinder/Text.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/params/Params.h"
#include "cinder/gl/Fbo.h"
#include "cinder/gl/GlslProg.h"
#include "cinder/Filesystem.h"
#include "ciUI.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class proj_map_arch : public AppNative {
public:
	void setup();
    
	void keyDown( KeyEvent event );
	void fileDrop( FileDropEvent event );
    
	void update();
	void draw();
    
    void prepareSettings( Settings *settings );
	void loadMovieFile( const fs::path &path );
    void findHomography(Vec2f src[4],Vec2f dst[4], float homography[16]);
    void gaussian_elimination(float *input, int n);
    void shutdown();
    void guiEvent(ciUIEvent *event);
    
	gl::Texture					mFrameTexture, mInfoTexture;
	qtime::MovieGl				mMovie;
    
    Vec2f snowTR, snowTL, snowBL, snowBR;
    Vec2f mask1TR, mask1TL, mask1BL, mask1BR;
    Vec2f mask2TR, mask2TL, mask2BL, mask2BR;
    Vec2f drawSize, screenSize;
    Vec2f offset;
    
   // params::InterfaceGl		mParams;
    bool saveSettings, loadSettings;
    string XMLfilename;
    
    ciUICanvas *gui;
    
    gl::Fbo fbo;
    bool showOutlines;
    bool showGui;
    
};
void proj_map_arch::prepareSettings( Settings *settings ){
    //settings->setWindowSize( 1920, 1080);
    settings->setWindowSize(1280, 720);
    settings->setFullScreen( false );
    settings->setResizable( true );
    settings->setFrameRate( 30.0f );
}
//---------------------------------------------------------------
void proj_map_arch::setup()
{
    //Set defaults pre-XML load
    snowTR = Vec2f(0,0);
    snowTL = Vec2f(1920,0);
    snowBL = Vec2f(1920,1080);
    snowBR = Vec2f(0,1080);
    mask1TR = Vec2f(0,0);
    mask1TL = Vec2f(1920,0);
    mask1BL = Vec2f(1920,1080);
    mask1BR = Vec2f(0,1080);
    mask2TR = Vec2f(0,0);
    mask2TL = Vec2f(1920,0);
    mask2BL = Vec2f(1920,1080);
    mask2BR = Vec2f(0,1080);
    drawSize =  Vec2f(1920,1080);
        screenSize =  Vec2f(1280,720);
    saveSettings = false;
    loadSettings = false;
    showOutlines = true;
    showGui = true;
    
    gui = new ciUICanvas(0,0,300,getWindowHeight());
    gui->addWidgetDown(new ciUILabel("Snow Map", CI_UI_FONT_SMALL));
    gui->addWidgetDown(new ciUILabelToggle(150, showOutlines, "ShowOutlines", CI_UI_FONT_SMALL));
    gui->addWidgetDown(new ciUI2DPad(150, 100, Vec2f(75,75), "SnowTR"));
    gui->addWidgetRight(new ciUI2DPad(150, 100, Vec2f(75,75), "SnowTL"));
    gui->addWidgetDown(new ciUI2DPad(150, 100, Vec2f(75,75), "SnowBR"));
    gui->addWidgetRight(new ciUI2DPad(150, 100, Vec2f(75,75), "SnowBL"));
    gui->addWidgetDown(new ciUI2DPad(150, 100, Vec2f(75,75), "Mask1TR"));
    gui->addWidgetRight(new ciUI2DPad(150, 100, Vec2f(75,75), "Mask1TL"));
    gui->addWidgetDown(new ciUI2DPad(150, 100, Vec2f(75,75), "Mask1BR"));
    gui->addWidgetRight(new ciUI2DPad(150, 100, Vec2f(75,75), "Mask1BL"));
    gui->addWidgetDown(new ciUI2DPad(150, 75, Vec2f(75,75), "Offset"));
    
    gui->registerUIEvents(this, &proj_map_arch::guiEvent);
    
    gl::Fbo::Format format;
	format.setSamples( 4 ); // uncomment this to enable 4x antialiasing
    format.enableColorBuffer(true,2);
    
    fbo = gl::Fbo(getWindowWidth(),getWindowHeight(), format);
    
	//fs::path moviePath = getAssetPath("photojpeg_codec.mov");
    //fs:path moviePath = "/Users/laser/Desktop/cinder_0.8.4_mac/Fakelove/proj_map_snow/assets/photojpeg_codec.mov"
	//if( ! moviePath.empty() )
    loadMovieFile(getResourcePath("photojpeg_codec.mov") );
}
//---------------------------------------------------------------
void proj_map_arch::keyDown( KeyEvent event )
{
	if( event.getChar() == 'f' ) {
		setFullScreen( ! isFullScreen() );
	}
    else if(event.getChar() == 'g'){
        showGui = !showGui;
    }
	else if( event.getChar() == 'o' ) {
		fs::path moviePath = getOpenFilePath();
		if( ! moviePath.empty() )
			loadMovieFile( moviePath );
	}
	else if( event.getChar() == '1' )
		mMovie.setRate( 0.5f );
	else if( event.getChar() == '2' )
		mMovie.setRate( 2 );
    
    if(event.getChar() == 's')
    {
        gui->saveSettings("guiSettings.xml");
    }
    else if(event.getChar() == 'l')
    {
        gui->loadSettings("guiSettings.xml");
    }
}
//---------------------------------------------------------------
void proj_map_arch::loadMovieFile( const fs::path &moviePath )
{
	try {
		// load up the movie, set it to loop, and begin playing
		mMovie = qtime::MovieGl( moviePath );
		mMovie.setLoop();
		mMovie.play();
    }
	catch( ... ) {
		console() << "Unable to load the movie." << std::endl;
		mMovie.reset();
	}
    
	mFrameTexture.reset();
}
//---------------------------------------------------------------
void proj_map_arch::fileDrop( FileDropEvent event )
{
	loadMovieFile( event.getFile( 0 ) );
}
//---------------------------------------------------------------
void proj_map_arch::update()
{
    gui->update();
    
	if( mMovie ){
		mFrameTexture = mMovie.getTexture();
    }
    
    
}
//---------------------------------------------------------------
void proj_map_arch::draw()
{
	gl::clear( Color( 0, 0, 0 ) );
	gl::enableAlphaBlending();
    
    //find warping for movie
    Vec2f snow_src[]={Vec2f(0,0),Vec2f(mMovie.getWidth(),0),Vec2f(mMovie.getWidth(),mMovie.getHeight()),Vec2f(0,mMovie.getHeight())};
    Vec2f snow_des[]={snowTR,snowTL,snowBL,snowBR};
    GLfloat snow_matrix[16];
    findHomography(snow_src,snow_des,snow_matrix);
    
    //find warping for rectangular mask1
    Vec2f mask1_src[]={Vec2f(0,0),Vec2f(mMovie.getWidth(),0),Vec2f(mMovie.getWidth(),mMovie.getHeight()),Vec2f(0,mMovie.getHeight())};
    Vec2f mask1_des[]={mask1TR,mask1TL,mask1BL,mask1BR};
    GLfloat mask1_matrix[16];
    findHomography(mask1_src,mask1_des,mask1_matrix);
    
    //find warping for rectangular mask2
    Vec2f mask2_src[]={Vec2f(0,0),Vec2f(mMovie.getWidth(),0),Vec2f(mMovie.getWidth(),mMovie.getHeight()),Vec2f(0,mMovie.getHeight())};
    Vec2f mask2_des[]={mask2TR,mask2TL,mask2BL,mask2BR};
    GLfloat mask2_matrix[16];
    findHomography(mask2_src,mask2_des,mask2_matrix);
    
    //draw into FBO
    fbo.bindFramebuffer();
    gl::clear();
    gl::pushMatrices();
    //flipflop_that_shit
    //MASK IT
    gl::clear();
    gl::translate(Vec3f(0,fbo.getHeight(),0));
    gl::rotate(Vec3f(0,180,180));
    glDisable(GL_BLEND);
    glColorMask(0, 0, 0, 1);
    glColor4f(1,1,1,1.0f);
    gl::pushMatrices();
    gl::pushModelView();
    gl::multModelView(mask1_matrix);
    gl::clear(ColorA(1,1,1,0.0f));
    //gl::color(1.0, 1.0, 1.0);
    gl::drawSolidRect(Rectf(0,0,drawSize.x,drawSize.y));
    gl::popModelView();
    gl::popMatrices();
    
    /*
    gl::pushMatrices();
    gl::pushModelView();
    gl::multModelView(mask2_matrix);
    //gl::clear(ColorA(1,1,1,0.0f));
    //gl::color(1.0, 1.0, 1.0);
    glColor4f(1,1,1,1.0f);
    
    gl::drawSolidRect(Rectf(0,0,drawSize.x,drawSize.y));
    gl::popModelView();
    gl::popMatrices();*/
    
    //draw the thing being masked by that shit
    glColorMask(1,1,1,0);
    glEnable(GL_BLEND);
    glBlendFunc( GL_DST_ALPHA, GL_ONE_MINUS_DST_ALPHA );
    if(mFrameTexture){
        gl::pushMatrices();
        gl::pushModelView();
        gl::multModelView(snow_matrix);
        //gl::clear(ColorA(1,1,1,0.0f));
        gl::color(1.0, 1.0, 1.0);
        gl::draw( mFrameTexture, Rectf(0,0,drawSize.x,drawSize.y)  );
        
        //gl::drawSolidRect(Rectf(0,0,1920,1080));
        gl::popModelView();
        gl::popMatrices();
        
    }
    gl::popMatrices();
    fbo.unbindFramebuffer();
    
    gl::pushMatrices();
    
    gl::draw(fbo.getTexture(), Rectf(offset.x,offset.y,getWindowWidth(),getWindowHeight()));
    gl::popMatrices();
    
    if(showOutlines){
        gl::color(1.0, 1.0, 0.0);
        
        gl::drawLine(mask1TL, mask1TR);
        gl::drawLine(mask1TR, mask1BR);
        gl::drawLine(mask1BR, mask1BL);
        gl::drawLine(mask1BL, mask1TL);
        
//        gl::drawLine(mask2TL, mask2TR);
//        gl::drawLine(mask2TR, mask2BR);
//        gl::drawLine(mask2BR, mask2BL);
//        gl::drawLine(mask2BL, mask2TL);
        
        gl::color(0.0, 1.0, 0.0);
        gl::drawLine(snowTL, snowTR);
        gl::drawLine(snowTR, snowBR);
        gl::drawLine(snowBR, snowBL);
        gl::drawLine(snowBL, snowTL);
    }
    
    if (showGui) {
        gui->draw();
    }
    
    
}

//--------------------------------------------------------------
void proj_map_arch::findHomography(Vec2f src[4], Vec2f dst[4], float homography[16]){
    // arturo castro - 08/01/2010
    //
    // create the equation system to be solved
    //
    // from: Multiple View Geometry in Computer Vision 2ed
    //       Hartley R. and Zisserman A.
    //
    // x' = xH
    // where H is the homography: a 3 by 3 matrix
    // that transformed to inhomogeneous coordinates for each point
    // gives the following equations for each point:
    //
    // x' * (h31*x + h32*y + h33) = h11*x + h12*y + h13
    // y' * (h31*x + h32*y + h33) = h21*x + h22*y + h23
    //
    // as the homography is scale independent we can let h33 be 1 (indeed any of the terms)
    // so for 4 points we have 8 equations for 8 terms to solve: h11 - h32
    // after ordering the terms it gives the following matrix
    // that can be solved with gaussian elimination:
    
    float P[8][9]={
        {-src[0].x, -src[0].y, -1,   0,   0,  0, src[0].x*dst[0].x, src[0].y*dst[0].x, -dst[0].x }, // h11
        {  0,   0,  0, -src[0].x, -src[0].y, -1, src[0].x*dst[0].y, src[0].y*dst[0].y, -dst[0].y }, // h12
        
        {-src[1].x, -src[1].y, -1,   0,   0,  0, src[1].x*dst[1].x, src[1].y*dst[1].x, -dst[1].x }, // h13
        {  0,   0,  0, -src[1].x, -src[1].y, -1, src[1].x*dst[1].y, src[1].y*dst[1].y, -dst[1].y }, // h21
        
        {-src[2].x, -src[2].y, -1,   0,   0,  0, src[2].x*dst[2].x, src[2].y*dst[2].x, -dst[2].x }, // h22
        {  0,   0,  0, -src[2].x, -src[2].y, -1, src[2].x*dst[2].y, src[2].y*dst[2].y, -dst[2].y }, // h23
        
        {-src[3].x, -src[3].y, -1,   0,   0,  0, src[3].x*dst[3].x, src[3].y*dst[3].x, -dst[3].x }, // h31
        {  0,   0,  0, -src[3].x, -src[3].y, -1, src[3].x*dst[3].y, src[3].y*dst[3].y, -dst[3].y }, // h32
    };
    
    gaussian_elimination(&P[0][0],9);
    
    // gaussian elimination gives the results of the equation system
    // in the last column of the original matrix.
    // opengl needs the transposed 4x4 matrix:
    float aux_H[]={ P[0][8],P[3][8],0,P[6][8], // h11  h21 0 h31
        P[1][8],P[4][8],0,P[7][8], // h12  h22 0 h32
        0      ,      0,0,0,       // 0    0   0 0
        P[2][8],P[5][8],0,1};      // h13  h23 0 h33
    
    for(int i=0;i<16;i++) homography[i] = aux_H[i];
}
//--------------------------------------------------------------
void proj_map_arch::gaussian_elimination(float *input, int n){
    // arturo castro - 08/01/2010
    //
    // ported to c from pseudocode in
    // http://en.wikipedia.org/wiki/Gaussian_elimination
    
    float * A = input;
    int i = 0;
    int j = 0;
    int m = n-1;
    while (i < m && j < n){
        // Find pivot in column j, starting in row i:
        int maxi = i;
        for(int k = i+1; k<m; k++){
            if(fabs(A[k*n+j]) > fabs(A[maxi*n+j])){
                maxi = k;
            }
        }
        if (A[maxi*n+j] != 0){
            //swap rows i and maxi, but do not change the value of i
            if(i!=maxi)
                for(int k=0;k<n;k++){
                    float aux = A[i*n+k];
                    A[i*n+k]=A[maxi*n+k];
                    A[maxi*n+k]=aux;
                }
            //Now A[i,j] will contain the old value of A[maxi,j].
            //divide each entry in row i by A[i,j]
            float A_ij=A[i*n+j];
            for(int k=0;k<n;k++){
                A[i*n+k]/=A_ij;
            }
            //Now A[i,j] will have the value 1.
            for(int u = i+1; u< m; u++){
                //subtract A[u,j] * row i from row u
                float A_uj = A[u*n+j];
                for(int k=0;k<n;k++){
                    A[u*n+k]-=A_uj*A[i*n+k];
                }
                //Now A[u,j] will be 0, since A[u,j] - A[i,j] * A[u,j] = A[u,j] - 1 * A[u,j] = 0.
            }
            
            i++;
        }
        j++;
    }
    
    //back substitution
    for(int i=m-2;i>=0;i--){
        for(int j=i+1;j<n-1;j++){
            A[i*n+m]-=A[i*n+j]*A[j*n+m];
            //A[i*n+j]=0;
        }
    }
}
//-----------
void proj_map_arch::shutdown(){
    delete gui;
}
//-----
void proj_map_arch::guiEvent(ciUIEvent *event){
    string name = event->widget->getName();
	
	if(name == "ShowOutlines")
	{
		ciUILabelToggle *toggle = (ciUILabelToggle *) event->widget;
        showOutlines = toggle->getValue();
	}
	else if(name == "SnowTR")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		snowTR.x = pad->getPercentValue().x*getWindowWidth();
		snowTR.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "SnowTL")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		snowTL.x = pad->getPercentValue().x*getWindowWidth();
		snowTL.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "SnowBL")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		snowBL.x = pad->getPercentValue().x*getWindowWidth();
		snowBL.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "SnowBR")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		snowBR.x = pad->getPercentValue().x*getWindowWidth();
		snowBR.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "Mask1TR")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		mask1TR.x = pad->getPercentValue().x*getWindowWidth();
		mask1TR.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "Mask1TL")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		mask1TL.x = pad->getPercentValue().x*getWindowWidth();
		mask1TL.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "Mask1BL")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		mask1BL.x = pad->getPercentValue().x*getWindowWidth();
		mask1BL.y = pad->getPercentValue().y*getWindowHeight();
	}
    else if(name == "Mask1BR")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		mask1BR.x = pad->getPercentValue().x*getWindowWidth();
		mask1BR.y = pad->getPercentValue().y*getWindowHeight();
	}

    else if(name == "Offset")
	{
		ciUI2DPad *pad = (ciUI2DPad *) event->widget;
		offset.x = pad->getPercentValue().x*getWindowWidth();
		offset.y = pad->getPercentValue().y*getWindowHeight();
	}

}

CINDER_APP_NATIVE( proj_map_arch, RendererGl(0) );
