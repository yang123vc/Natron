//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#ifndef GL_VIEWER_HEADER__
#define GL_VIEWER_HEADER__


#include "Superviser/gl_OsDependent.h" // <must be included before QGlWidget because of gl.h and glew.h

#include <QtOpenGL/QGLWidget>
#include <QtCore/QEvent>
#include <QtCore/QTimer>
#include <QtGui/QKeyEvent>
#include <string>
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <QtOpenGL/QGLFormat>
#include <ImfThreading.h>
#include <ImfInputFile.h>
#include <ImfChannelList.h>
#include <ImfRgbaYca.h>
#include <ImfArray.h>
#include <QtOpenGL/QGLShaderProgram>
#include <QtGui/qvector4d.h>
#include <QtGui/QPainter>
#include "Core/row.h"
#include "Core/displayFormat.h"
#include "Gui/textRenderer.h"
#include "Core/viewercache.h"
#include "Core/viewerNode.h"

#ifndef PW_DEBUG
#define checkGLErrors() ((void)0)
#else
#define checkGLErrors() \
{ \
	GLenum error = glGetError(); \
	if(error != GL_NO_ERROR) { \
	std::cout << "GL_ERROR :" << __FILE__ << " "<< __LINE__ << " " << gluErrorString(error) << std::endl; \
	} \
}
#endif

class Lut;
class InfoViewerWidget;
class Controler;
class TextureCache;
class ViewerGL : public QGLWidget
{
	Q_OBJECT

		/*In order to avoid the overhead of lengthy function
		calls, the diskcache and the videoEngine have direct access
		to this class. This is an exception and it is the case only
		for performance.*/
		friend class ViewerCache;
	friend class VideoEngine;

	/*basic state switching for mouse events*/
	enum MOUSE_STATE{DRAGGING,UNDEFINED};
    
    /*enum used by the handleTextureAndViewerCache function to determine the behaviour
     to have depending on the caching mode*/
    enum CACHING_MODE{TEXTURE_CACHE,VIEWER_CACHE};

	class ZoomContext{
	public:
		ZoomContext():zoomX(0),zoomY(0),zoomFactor(1),restToZoomX(0),restToZoomY(0)
		{}

		QPointF old_zoomed_pt,old_zoomed_pt_win;
		float zoomX,zoomY;
		float restToZoomX,restToZoomY;
		float zoomFactor;
	
		void setZoomXY(float x,float y){zoomX=x;zoomY=y;}
		std::pair<int,int> getZoomXY(){return std::make_pair(zoomX,zoomY);}
		/*the level of zoom used to display the frame*/
		void setZoomFactor(float f){zoomFactor = f;}
		float getZoomFactor(){return zoomFactor;}
	};

public:
	/*3 different constructors, that all take a different parameter related to OpenGL or Qt widget parenting*/
	ViewerGL(float byteMode,QWidget* parent=0, const QGLWidget* shareWidget=NULL);
	ViewerGL(float byteMode,const QGLFormat& format,QWidget* parent=NULL, const QGLWidget* shareWidget=NULL);
	ViewerGL(float byteMode,QGLContext* context,QWidget* parent=0, const QGLWidget* shareWidget=NULL);

	virtual ~ViewerGL();

	/*drawRow is called by the viewer node. Row is a pointer to the
	row to draw.*/
	void drawRow(Row *row);

	void checkFrameBufferCompleteness(const char where[],bool silent=true);

	/*initiliazes OpenGL context related stuff*/
	void initializeGL();

	/*check extensions and init Glew for windows*/
	void initAndCheckGlExtensions ();

	/*handles the resizing of the viewer*/
	void resizeGL(int width,int height);

	/*drawing means we will display an image, otherwise it is black.*/
	void drawing(bool d){_drawing=d;if(!_drawing) _firstTime=true;}
	bool drawing()const{return _drawing;}

	/*Convenience function.
	 *Ydirection is the order of fill of the display texture:
	either bottom to top or top to bottom.*/
	int Ydirection(){return getCurrentViewerInfos()->getYdirection();}

	/*handy functions to save/restore the GL context states*/
	void saveGLState();
	void restoreGLState();
	void makeCurrent();

	/*rgbMode is true when we have rgba data. 
	False means it is luminance chroma*/
	bool rgbMode(){return getCurrentViewerInfos()->rgbMode();}

	/*the lut used by the viewer to output images*/
	float lutType(){return _lut;}


	/*the exposure applied to the fragments when drawing*/
	float getExposure(){return exposure;}

	/*these are the channels the viewer wants to display*/
	ChannelMask displayChannels(){return _channelsToDraw;}

	/*The dataWindow of the currentFrame(BBOX)*/
	const Box2D& dataWindow(){return getCurrentViewerInfos()->getDataWindow();}

	/*The displayWindow of the currentFrame(Resolution)*/
	const Format& displayWindow(){return getCurrentViewerInfos()->getDisplayWindow();}

	/*(ROI), not used yet*/
	Box2D& regionOfInterest(){return _roi;}
	void regionOfInterest(Box2D& renderBox){ this->_roi = renderBox;}

	/*function initialising display textures (and glsl shader the first time)
	according to the current level of zooom*/
	void initTextures(int w,int h,GLuint texID);
	/*init the RGB texture*/
	void initTextureBGRA(int w,int h,GLuint texID);
	/*width and height of the display texture*/
	std::pair<int,int> textureSize(){return _textureSize;}

	/*start using the RGB shader to display the frame*/
	void activateShaderRGB();

	/*start using the luminance/chroma shader*/
	void activateShaderLC();


	/*overload of QT enter/leave/resize events*/
	void enterEvent(QEvent *event)
	{   QGLWidget::enterEvent(event);
        setFocus();
        grabMouse();
        grabKeyboard();
	}
	void leaveEvent(QEvent *event)
	{
		QGLWidget::leaveEvent(event);
		setFocus();
        releaseMouse();
		releaseKeyboard();
	}
	virtual void resizeEvent(QResizeEvent* event){ // public to hack the protected field
		makeCurrent();
		if(isVisible()){
			QGLWidget::resizeEvent(event);
			setVisible(true);
		}
	}
	/*Convenience functions to communicate with the ZoomContext*/
    float zoomedHeight(){return floor((float)displayWindow().h()*_zoomCtx.zoomFactor);}
    float zoomedWidth(){return floor((float)displayWindow().w()*_zoomCtx.zoomFactor);}
	void setZoomFactor(float f){_zoomCtx.setZoomFactor(f); emit zoomChanged(f*100);}
	float getZoomFactor(){return _zoomCtx.getZoomFactor();}
    
    /*computes what are the rows that should be displayed on viewer
     for the given displayWindow with the  zoom factor and  current zoom center.
	 They will be stored from bottom to top. The values are returned
     as a map of displayWindow coordinates mapped to viewport cooridnates*/
    std::map<int,int> computeRowSpan(Format displayWindow,float zoomFactor);
	/*computes the inverse matrix of m and stores it in out*/
    int _glInvertMatrix(float *m, float *out);
	/*multiply matrix1 by matrix2 and stores the result in result*/
    void _glMultMats44(float *result, float *matrix1, float *matrix2);
	/*multiply the matrix by the vector and stores it in resultvector*/
    void _glMultMat44Vect(float *resultvector, const float *matrix, const float *pvector);
	/*Multiplies matrix by pvector and stores only the ycomponent (multiplied by the homogenous coordinates)*/
    int _glMultMat44Vect_onlyYComponent(float *yComponent, const float *matrix, const float *pvector);
	/*translation/zoom related functions*/
	void setTranslation(float x,float y){transX = x; transY=y;}
	std::pair<int,int> getTranslation(){return std::make_pair(transX,transY);}
	void resetMousePos(){ new_pos.setX(0);new_pos.setY(0); old_pos.setX(0);old_pos.setY(0);}
	

	/*the file type of the current frame*/
	void fileType(File_Type f){_filetype=f;}
	File_Type fileType(){return _filetype;}

	/*Fill the frameData buffer with the current row . It converts
	32bit floating points intensities to 8bit using error diffusion
	algorithm. It also applies the viewer LUT during the filling process.*/
	void convertRowToFitTextureBGRA(const float* r,const float* g,const float* b,
		int w,int yOffset,const float* alpha);
	/*idem above but for floating point textures (no dithering applied here)*/
	void convertRowToFitTextureBGRA_fp(const float* r,const float* g,const float* b,
		int w,int yOffset,const float* alpha);


	/*handy functions to fill textures*/
	static U32 toBGRA(U32 r,U32 g,U32 b,U32 a);
	static QVector4D U32toBGRA(U32& c);

	/*returns mouse position in window coordinates
	Takes in input openGL coords.*/
	QPoint mousePosFromOpenGL(int x,int y);

	/*returns openGL coordinates of the mouse position
	passed in window coordinates*/
	QVector3D openGLpos(int x,int y);
    
    /*same as openGLpos(x,y) but ignores the z component,avoiding
     a call to glReadPixels()*/
    QPoint openGLpos_fast(int x,int y);
    
	/*get color of the pixel located at (x,y) in
	opengl coordinates*/
	QVector4D getColorUnderMouse(int x,int y);

	/*set the infoviewer pointer(the line displaying
	infos below the viewer)*/
	void setInfoViewer(InfoViewerWidget* i );

	/*handy function that zoom automatically the viewer so it fit
	the displayWindow  entirely in the viewer*/
	void fitToFormat(Format displayWindow);

	/*display black in the viewer*/
	void clearViewer(){
		_drawing = false;
		updateGL();
	}

	/*set the readerInfo to be blank(used when
	displaying black)*/
	void blankInfoForViewer(bool onInit=false);

	/*viewerInfo related functions)*/
	void setCurrentViewerInfos(Viewer::ViewerInfos *viewerInfos,bool onInit=false,bool initBoundaries = false);
    Viewer::ViewerInfos* getCurrentViewerInfos(){return _currentViewerInfos;}

	void updateCurrentViewerInfo(){updateDataWindowAndDisplayWindowInfo();}


	/*update the BBOX info*/
	void updateDataWindowAndDisplayWindowInfo();

	/*turn on/off overlay*/
	void turnOnOverlay(){_overlay=true;}
	void turnOffOverlay(){_overlay=false;}


	/*called by the main thread to init specific variables per frame
	* safe place to do things before any computation for the frame is started. Avoid heavy computations here.
	* That's where the viewer cached frame is initialised.
    * if a texture residing in the texture cache already holds the results.
    * The caller should then abort any computation and set for current texture the one returned.
    * otherwise returns false.
	*/
	bool handleTextureAndViewerCache(std::string filename,int nbFrameHint,int w,int h,ViewerGL::CACHING_MODE mode);

	std::pair<int,int> getTextureSize(){return _textureSize;}
    
    /*Allocate the pbo represented by the pboID with dataSize bytes.
     This function returns a pointer to
     the mapping created between the GPU and the RAM*/
	void* allocateAndMapPBO(size_t dataSize,GLuint pboID);

	/*Fill the mapped PBO represented by dst with byteCount of src*/
	void fillPBO(const char* src,void* dst,size_t byteCount);

	/*Unmap the current mapped PBO and copies it to the display texture*/
	void copyPBOtoTexture(int w,int h);

	void doEmitFrameChanged(int f){
		emit frameChanged(f);
	}

	/*Returns a pointer to the data of the current frame.*/
	const char* getFrameData(){return frameData;}

	GLuint getPBOId(int index){return texBuffer[index];}
    
    void setRowSpan(std::pair<int,int> p){_rowSpan = p;}
    
    void setTextureCache(TextureCache* cache){ _textureCache = cache;}
    
    TextureCache* getTextureCache(){return _textureCache;}
    
    void setCurrentTexture(GLuint texture){currentTexture = texture;}
    
    U32 getCurrentTexture(){return currentTexture;}
    
    GLuint getDefaultTextureID(){return texId[0];}
    
    float byteMode(){return _byteMode;}
    
    bool hasHardware(){return _hasHW;}
    
    ViewerCache::FrameID& getCurrentFrameID(){return frameInfo;}
    
    void disconnectViewer();

	public slots:
		virtual void updateGL();
		void updateColorSpace(QString str);
		void zoomSlot(int v);
		void zoomSlot(double v){zoomSlot((int)v);} // convenience for FeedBackSpinBox
		void updateExposure(double);
signals:
		void infoMousePosChanged();
		void infoColorUnderMouseChanged();
		void infoResolutionChanged();
		void infoDisplayWindowChanged();
		void zoomChanged(int v);
		void frameChanged(int);


protected :

	virtual void paintGL();
	void drawOverlay();
	virtual void paintEvent(QPaintEvent* event);
	virtual void keyPressEvent ( QKeyEvent * event );
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void mouseMoveEvent(QMouseEvent *event);
	void wheelEvent(QWheelEvent *event);

private:
	void initConstructor(); // called in the constructor
	void initShaderGLSL(); // init shaders
	int isExtensionSupported(const char* extension); // check if OpenGL extension is supported
    
	void initBlackTex();// init the black texture when viewer is disconnected
	void drawBlackTex(); // draw the black texture


	TextRenderer _textRenderer;

	GLuint texBuffer[2]; // PBO's used to upload textures
	GLuint texId[1]; // < the texture used for rendering when the texture does not come from cache
    GLuint currentTexture; // the texture used for rendering, might come from the cache,otherwise it is texId[0]
	//  void* _renderingBuffer; // the frame currently computed (mapped PBO handle)
	std::pair<int,int> _textureSize; // size of texId[0]
	GLuint texBlack[1];
	QGLShaderProgram* shaderRGB;
	QGLShaderProgram* shaderLC;
	QGLShaderProgram* shaderBlack;
	bool _shaderLoaded;


	float _lut; // 0 = NONE , 1 = sRGB , 2 = Rec 709  : this value is used by shaders
	Lut* _colorSpace; //the lut used to do the viewer colorspace conversion when we can't use shaders
    bool _usingColorSpace;

	int Ysampling; // sampling factor for LC files

	InfoViewerWidget* _infoViewer;
	Box2D _roi;  // region of interest
	float exposure ;		// Current exposure setting.  All pixels
	// are multiplied by pow(2,exposure) before
	// they appear on the screen
	ChannelMask _channelsToDraw;

    Viewer::ViewerInfos* _currentViewerInfos;
    
    Viewer::ViewerInfos* _blankViewerInfos;



	bool _drawing;
	bool _firstTime;
	File_Type _filetype;

	MOUSE_STATE _ms;
	QPointF old_pos;
	QPointF new_pos;
	float transX,transY;
    
    std::pair<int,int> _rowSpan;

	ZoomContext _zoomCtx;


	QString _resolutionOverlay;
	QString _btmLeftBBOXoverlay;
	QString _topRightBBOXoverlay;

	bool _overlay;
	float _byteMode; // boolean
	bool _hasHW;
	bool _fullscreen;

	char* frameData; // last frame computed data , either U32* or float* depending on _byteMode
    bool _mustFreeFrameData; // true whenever the texture displayed is just a portion of the full image
    bool _noDataTransfer; // true whenever the current texture already holds data and doesn't need
    //a copy from a pbo
    
    
	ViewerCache::FrameID frameInfo;
    
    TextureCache* _textureCache;
    

};
#endif // GLVIEWER_H
