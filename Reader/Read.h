//  Powiter
//
//  Created by Alexandre Gauthier-Foichat on 06/12
//  Copyright (c) 2013 Alexandre Gauthier-Foichat. All rights reserved.
//  contact: immarespond at gmail dot com
#ifndef __READ__H__
#define __READ__H__

#include <QtCore/QString>
#include <QtCore/QChar>
#include <map>
#include <QtCore/QStringList>
#include "Core/channels.h"
#include "Core/row.h"
#include "Core/displayFormat.h"
#include <QtGui/QRgb>
#include "Reader/Reader.h"
class ViewerGL;
class Lut;
class Read{
    
public:
    /*Constructors should initialize variables, but shouldn't do any heavy computations, as these objects
     are oftenly re-created. To initialize the input color-space , you can do so by overloading
     initializeColorSpace. This function is called after the constructor and before any
     reading occurs.*/
	Read(Reader* op);
    
	virtual ~Read();
    
    /*Should return the list of file types supported by the decoder: "png","jpg", etc..*/
    virtual std::vector<std::string> fileTypesDecoded()=0;
    
    /*Should return the name of the reader : "ffmpeg", "OpenEXR" ...*/
    virtual std::string decoderName()=0;
    
    /*This must be implemented to do the to linear colorspace conversion*/
	virtual void engine(int y,int offset,int range,ChannelMask channels,Row* out)=0;
    
    /*can be overloaded to add knobs dynamically to the reader depending on the file type*/
	virtual void createKnobDynamically();
    
    /*Must be implemented to tell whether this file type supports stereovision*/
	virtual bool supports_stereo()=0;
    
//    /*Must be implemented. This function opens the file specified by filename, reads
//     all the meta-data and extracts all the data. If openBothViews is on, this function
//     should read both views of the file.
//     More precise documentation coming soon.*/
//    virtual void open(const QString filename,bool openBothViews = false)=0;
    
    /*This function  calls readHeader and readAllData*/
    void readData(const QString filename,bool openBothViews = false);
    
    /*This function calls readScanLine for the requested scanLines. It does not call readHeader.
     If onlyExtaRows is true, this function reads only the content of the _rowsToCompute member
     of slContext. Otherwise, it will compute the rows contained in the _rows of slContext.*/
    void readScanLineData(const QString filename,
                          Reader::Buffer::ScanLineContext* slContext,
                          bool onlyExtraRows,
                          bool openBothViews = false);
    
    /*Should open the file and call setReaderInfo with the infos from the file.*/
    virtual void readHeader(const QString filename,bool openBothViews)=0;
    
    /*Must be implemented to know whether this Read* supports reading only
     scanlines. In the case it does,the engine can be must faster by reading
     only the lines that are needed.*/
    virtual bool supportsScanLine()=0;
    
    /*Must be overloaded by reader that supports scanline. It should return
     the current count of scanlines read by the Read* */
    virtual int scanLinesCount(){return 0;}
    
    /*Should read the already opened file and extract all the data for either 1 view or
     2 views. By default does nothing, you should either overload this function or readScanLine.*/
    virtual void readAllData(bool openBothViews){}
    
    /*Must be implemented if supportsScanLine() returns true. It should
     read one scan line from the file.By default does nothing, you should
     either overload this function or readScanLine*/
    virtual void readScanLine(int y){}
    
    /*Must be overloaded: this function is used to create a little preview that
     can be seen on the GUI node*/
    virtual void make_preview()=0;
    
    /*Returns true if the file is stereo*/
    bool fileStereo(){return is_stereo;};
    
    /*Must implement it to initialize the appropriate colorspace  for
     the file type. You can initialize the _lut member by calling the
     function Lut::getLut(datatype) */
    virtual void initializeColorSpace()=0;
    
    /*Returns true if the file has alpha premultiplied data*/
    bool premultiplied(){return _premult;}
    
    /*Returns true if the user checked autoAlpha:
     this feature creates automatically an empty alpha channel*/
    bool autoAlpha(){return _autoCreateAlpha;}
    
    /*Returns the reader colorspace*/
    Lut* lut(){return _lut;}
    
    /*This function should be call at the end of open(...)
     It set all the reader infos necessary for the read frame.*/
	void setReaderInfo(Format dispW,
                       Box2D dataW,
                       QString file,
                       ChannelMask channels,
                       int Ydirection ,
                       bool rgb );
    
    /*Returns all the infos necessary for the current frame*/
    ReaderInfo* getReaderInfo(){return _readInfo;}
    
protected:
    
    void from_byte(Channel z, float* to, const uchar* from, const uchar* alpha, int W, int delta = 1);
    void from_byteQt(Channel z, float* to, const QRgb* from, int W, int delta = 1);
    void from_short(Channel z, float* to, const U16* from, const U16* alpha, int W, int bits, int delta = 1);
    void from_float(Channel z, float* to, const float* from, const float* alpha, int W, int delta = 1);
   // void from_byte(float* r,float* g,float* b, const uchar* from, bool hasAlpha, int W, int delta ,float* a,bool qtbuf = true);
   
   // void from_short(float* r,float* g,float* b, const U16* from, const U16* alpha, int W, int bits, int delta ,float* a);
   
//	void from_float(float* r,float* g,float* b, const float* fromR,const float* fromG,
 //                   const float* fromB, int W, int delta ,const float* fromA,float* a);
    
        
	bool is_stereo;
    bool _premult; //if the file contains a premultiplied 4 channel image, this must be turned-on
    bool _autoCreateAlpha;
	Reader* op;
    Lut* _lut;
    ReaderInfo* _readInfo;
    
};


typedef Read* (*ReadBuilder)(void*);
/*Classes deriving Read should implement a function named BuildRead with the following signature:
static Read* BuildRead(Reader*);
 */


#endif // __READ__H__