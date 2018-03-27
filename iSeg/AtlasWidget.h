/*
 * Copyright (c) 2018 The Foundation for Research on Information Technologies in Society (IT'IS).
 * 
 * This file is part of iSEG
 * (see https://github.com/ITISFoundation/osparc-iseg).
 * 
 * This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 */
#ifndef ATLAS_WIDGET
#define ATLAS_WIDGET

#include "widgetcollection.h"
#include <QScrollArea>
#include <q3action.h>
#include <q3vbox.h>
#include <qcheckbox.h>
#include <qevent.h>
#include <qimage.h>
#include <qlabel.h>
#include <qpoint.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qwidget.h>
//Added by qt3to4:
#include "Core/Pair.h"
#include "Core/Point.h"
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QWheelEvent>
#include <vector>

#include "AtlasViewer.h"

class AtlasWidget : public QWidget
{
	Q_OBJECT
public:
	AtlasWidget(const char *filename, QDir picpath, QWidget *parent = 0,
							const char *name = 0, Qt::WindowFlags wFlags = 0);
	~AtlasWidget();
	bool isOK;

private:
	QLabel *lb_contrast;
	QLabel *lb_brightness;
	QLabel *lb_transp;
	QLabel *lb_name;
	QSlider *sl_contrast;
	QSlider *sl_brightness;
	QSlider *sl_transp;
	AtlasViewer *atlasViewer;
	QScrollArea *sa_viewer;
	zoomer_widget *zoomer;
	QScrollBar *scb_slicenr;
	QButtonGroup *bg_orient;
	QRadioButton *rb_x;
	QRadioButton *rb_y;
	QRadioButton *rb_z;
	QHBoxLayout *hbox1;
	QHBoxLayout *hbox2;
	QHBoxLayout *hbox3;
	QVBoxLayout *vbox1;

	bool loadfile(const char *filename);

	float *image;
	tissues_size_t *tissue;
	float minval, maxval;
	float dx, dy, dz;
	unsigned short dimx, dimy, dimz;
	std::vector<float> color_r;
	std::vector<float> color_g;
	std::vector<float> color_b;
	std::vector<QString> tissue_names;

	QDir m_picpath;

private slots:
	void scb_slicenr_changed();
	void sl_transp_changed();
	void xyz_changed();
	void pt_moved(tissues_size_t val);
	void sl_brightcontr_moved();
};

#endif //ATLAS_WIDGET