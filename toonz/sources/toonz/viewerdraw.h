#pragma once

#ifndef VIEWERDRAW_INCLUDED
#define VIEWERDRAW_INCLUDED

//
// funzioni per il disegno (OpenGL) di oggetti
// che si vedono nel viewer
//

#include <vector>
#include "tgeometry.h"
#include "tfilepath.h"
#include "tsystem.h"
#include "tenv.h"
#include "toonz/txshsimplelevel.h"

// forward declaration
class SceneViewer;
class Ruler;
class TAffine;

//=============================================================================
// ViewerDraw
//-----------------------------------------------------------------------------

namespace ViewerDraw {

enum {  // cfr drawCamera()
  CAMERA_REFERENCE = 0X1,
  CAMERA_3D        = 0X2,
  SAFE_AREA        = 0X4,
  SOLID_LINE       = 0X8,
  SUBCAMERA        = 0X10
};

TRectD getCameraRect();

void drawCameraMask(SceneViewer *viewer);
void drawGridAndGuides(SceneViewer *viewer, double viewerScale, Ruler *vRuler,
                       Ruler *hRuler, bool gridEnabled);

void draw3DCamera(unsigned long flags, double zmin, double phi);
void drawCamera(unsigned long flags, double pixelSize);

void draw3DFrame(double zmin, double phi);
void drawDisk(int &tableDLId);
void drawFieldGuide();
void drawColorcard(UCHAR channel);

void drawFrames(SceneViewer *viewer, bool levelEditing);

unsigned int createDiskDisplayList();


// Add to the back of current scene's frames preset name
const QString RELOAD_TAG = "@@RELOAD_FORCE";

void getFramesPreset(QList<QList<double>>& _sizeList,
    TXshSimpleLevel** _slP = nullptr,
    TPointD* _offsetP = nullptr);

static inline TFilePath getFramesIniPath() {
  TFilePath fp         = TEnv::getConfigDir();
  std::string fileName = "framespresets.ini";
  TFilePath searchPath = fp;

  while (!TFileStatus(searchPath + fileName).doesExist() &&
         !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
    searchPath = searchPath.getParentDir();
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fileName   = "safearea.ini";
    searchPath = fp;

    while (!TFileStatus(searchPath + fileName).doesExist() &&
           !searchPath.isRoot() && searchPath.getParentDir() != TFilePath()) {
      searchPath = searchPath.getParentDir();
    }
  }

  if (!TFileStatus(searchPath + fileName).doesExist()) {
    fp = fp + "framespresets.ini";
    TSystem::touchFile(fp);
  } else
    fp = searchPath + fileName;

  return fp;
}

}  // namespace ViewerDraw

#endif
