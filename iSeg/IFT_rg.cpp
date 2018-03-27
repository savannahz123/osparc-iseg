/*
 * Copyright (c) 2018 The Foundation for Research on Information Technologies in Society (IT'IS).
 * 
 * This file is part of iSEG
 * (see https://github.com/ITISFoundation/osparc-iseg).
 * 
 * This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 */
#include "Precompiled.h"

#include "FormatTooltip.h"
#include "IFT_rg.h"
#include "SlicesHandler.h"
#include "bmp_read_1.h"

#include "Core/IFT2.h"
#include "Core/linedraw.h"

#include <q3hbox.h>
#include <q3vbox.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qslider.h>
#include <qwidget.h>

#include <algorithm>

using namespace std;

IFTrg_widget::IFTrg_widget(SlicesHandler *hand3D, QWidget *parent,
													 const char *name, Qt::WindowFlags wFlags)
		: QWidget1(parent, name, wFlags), handler3D(hand3D)
{
	setToolTip(Format(
			"Segment multiple tissues by drawing lines in the current slice based on "
			"the Image Foresting Transform. "
			"These lines are drawn with the color of the currently selected tissue. "
			"Multiple lines of different colours can be drawn "
			"and they are subsequently used as seeds to grow regions based on a "
			"local homogeneity criterion. Through competitive growing the best "
			"boundaries "
			"between regions grown from lines with different colours are identified."
			"<br>"
			"The result is stored in the Target. To assign a segmented region to a "
			"tissue the 'Adder' must be used."));

	activeslice = handler3D->get_activeslice();
	bmphand = handler3D->get_activebmphandler();

	area = 0;

	vbox1 = new Q3VBox(this);

	pushclear = new QPushButton("Clear Lines", vbox1);
	pushremove = new QPushButton("Remove Line", vbox1);
	pushremove->setToggleButton(true);
	pushremove->setToolTip(
			Format("Remove Line followed by a click on a line deletes "
						 "this line and automatically updates the segmentation. If Remove "
						 "Line has "
						 "been pressed accidentally, a second press will deactivate the "
						 "function again."));

	hbox1 = new Q3HBox(vbox1);

	sl_thresh = new QSlider(Qt::Horizontal, vbox1);
	sl_thresh->setRange(0, 100);
	sl_thresh->setValue(60);
	sl_thresh->setEnabled(false);
	sl_thresh->setFixedWidth(400);

	hbox1->setFixedSize(hbox1->sizeHint());
	vbox1->setFixedSize(vbox1->sizeHint());

	IFTrg = NULL;
	lbmap = NULL;
	thresh = 0;

	QObject::connect(pushclear, SIGNAL(clicked()), this, SLOT(clearmarks()));
	QObject::connect(sl_thresh, SIGNAL(sliderMoved(int)), this,
									 SLOT(slider_changed(int)));
	QObject::connect(sl_thresh, SIGNAL(sliderPressed()), this,
									 SLOT(slider_pressed()));
	QObject::connect(sl_thresh, SIGNAL(sliderReleased()), this,
									 SLOT(slider_released()));
}

IFTrg_widget::~IFTrg_widget()
{
	delete vbox1;

	if (IFTrg != NULL)
		delete IFTrg;
	if (lbmap != NULL)
		delete lbmap;
	return;
}

void IFTrg_widget::init()
{
	if (activeslice != handler3D->get_activeslice())
	{
		activeslice = handler3D->get_activeslice();
		bmphand = handler3D->get_activebmphandler();
		init1();
		if (sl_thresh->isEnabled())
		{
			getrange();
		}
	}
	else
		init1();

	hideparams_changed();

	return;
}

void IFTrg_widget::newloaded()
{
	activeslice = handler3D->get_activeslice();
	bmphand = handler3D->get_activebmphandler();
}

void IFTrg_widget::init1()
{
	vector<vector<mark>> *vvm = bmphand->return_vvm();
	vm.clear();
	for (vector<vector<mark>>::iterator it = vvm->begin(); it != vvm->end(); it++)
	{
		vm.insert(vm.end(), it->begin(), it->end());
		;
	}
	emit vm_changed(&vm);
	area = bmphand->return_height() * (unsigned)bmphand->return_width();
	if (lbmap != NULL)
		free(lbmap);
	lbmap = (float *)malloc(sizeof(float) * area);
	for (unsigned i = 0; i < area; i++)
		lbmap[i] = 0;
	unsigned width = (unsigned)bmphand->return_width();
	for (vector<mark>::iterator it = vm.begin(); it != vm.end(); it++)
	{
		lbmap[width * it->p.py + it->p.px] = (float)it->mark;
	}
	for (vector<Point>::iterator it = vmdyn.begin(); it != vmdyn.end(); it++)
	{
		lbmap[width * it->py + it->px] = (float)tissuenr;
	}

	if (IFTrg != NULL)
		delete (IFTrg);
	IFTrg = bmphand->IFTrg_init(lbmap);

	thresh = 0;

	if (!vm.empty())
		sl_thresh->setEnabled(true);
}

void IFTrg_widget::cleanup()
{
	vmdyn.clear();
	if (IFTrg != NULL)
		delete IFTrg;
	if (lbmap != NULL)
		delete lbmap;
	IFTrg = NULL;
	lbmap = NULL;
	sl_thresh->setEnabled(false);
	emit vmdyn_changed(&vmdyn);
	emit vm_changed(&vmempty);
}

void IFTrg_widget::tissuenr_changed(int i)
{
	tissuenr = (unsigned)i + 1;
	return;
}

void IFTrg_widget::mouse_clicked(Point p)
{
	last_pt = p;
	if (pushremove->isOn())
	{
		removemarks(p);
	}
}

void IFTrg_widget::mouse_moved(Point p)
{
	if (!pushremove->isOn())
	{
		addLine(&vmdyn, last_pt, p);
		last_pt = p;
		emit vmdyn_changed(&vmdyn);
	}
}

void IFTrg_widget::mouse_released(Point p)
{
	if (!pushremove->isOn())
	{
		addLine(&vmdyn, last_pt, p);
		mark m;
		m.mark = tissuenr;
		unsigned width = (unsigned)bmphand->return_width();
		vector<mark> vmdummy;
		vmdummy.clear();
		for (vector<Point>::iterator it = vmdyn.begin(); it != vmdyn.end(); it++)
		{
			m.p = *it;
			vmdummy.push_back(m);
			lbmap[it->px + width * it->py] = tissuenr;
		}
		vm.insert(vm.end(), vmdummy.begin(), vmdummy.end());

		common::DataSelection dataSelection;
		dataSelection.sliceNr = handler3D->get_activeslice();
		dataSelection.work = true;
		dataSelection.vvm = true;
		emit begin_datachange(dataSelection, this);

		bmphand->add_vm(&vmdummy);

		vmdyn.clear();
		emit vmdyn_changed(&vmdyn);
		emit vm_changed(&vm);
		execute();

		emit end_datachange(this);
	}
	else
	{
		pushremove->setOn(false);
	}
}

void IFTrg_widget::execute()
{
	IFTrg->reinit(lbmap, false);
	if (hideparams)
		thresh = 0;
	getrange();
	float *f1 = IFTrg->return_lb();
	float *f2 = IFTrg->return_pf();
	float *work_bits = bmphand->return_work();

	float d = 255.0f / bmphand->return_vvmmaxim();
	for (unsigned i = 0; i < area; i++)
	{
		if (f2[i] < thresh)
			work_bits[i] = f1[i] * d;
		else
			work_bits[i] = 0;
	}
	sl_thresh->setEnabled(true);

	bmphand->set_mode(2, false);
}

void IFTrg_widget::clearmarks()
{
	for (unsigned i = 0; i < area; i++)
		lbmap[i] = 0;

	vm.clear();
	vmdyn.clear();
	bmphand->clear_vvm();
	emit vmdyn_changed(&vmdyn);
	emit vm_changed(&vm);
}

void IFTrg_widget::slider_changed(int i)
{
	thresh = i * 0.01f * maxthresh;
	if (IFTrg != NULL)
	{
		float *f1 = IFTrg->return_lb();
		float *f2 = IFTrg->return_pf();
		float *work_bits = bmphand->return_work();

		float d = 255.0f / bmphand->return_vvmmaxim();
		for (unsigned i = 0; i < area; i++)
		{
			if (f2[i] < thresh)
				work_bits[i] = f1[i] * d;
			else
				work_bits[i] = 0;
		}
		bmphand->set_mode(2, false);
		emit end_datachange(this, common::NoUndo);
	}
}

void IFTrg_widget::bmp_changed()
{
	//	cleanup();
	bmphand = handler3D->get_activebmphandler();
	sl_thresh->setEnabled(false);
	init1();
}

void IFTrg_widget::slicenr_changed()
{
	//	if(activeslice!=handler3D->get_activeslice()){
	activeslice = handler3D->get_activeslice();
	bmphand_changed(handler3D->get_activebmphandler());
	//	}
}

void IFTrg_widget::bmphand_changed(bmphandler *bmph)
{
	bmphand = bmph;

	unsigned width = (unsigned)bmphand->return_width();
	/*	for(vector<mark>::iterator it=vm.begin();it!=vm.end();it++){
		lbmap[width*it->p.py+it->p.px]=0;
	}*/

	vector<vector<mark>> *vvm = bmphand->return_vvm();
	vm.clear();
	for (vector<vector<mark>>::iterator it = vvm->begin(); it != vvm->end(); it++)
	{
		vm.insert(vm.end(), it->begin(), it->end());
		;
	}

	for (unsigned i = 0; i < area; i++)
		lbmap[i] = 0;
	for (vector<mark>::iterator it = vm.begin(); it != vm.end(); it++)
	{
		lbmap[width * it->p.py + it->p.px] = (float)it->mark;
	}

	if (IFTrg != NULL)
		delete (IFTrg);
	IFTrg = bmphand->IFTrg_init(lbmap);

	//	thresh=0;

	if (sl_thresh->isEnabled())
	{
		getrange();
	}

	emit vm_changed(&vm);

	return;
}

void IFTrg_widget::getrange()
{
	float *pf = IFTrg->return_pf();
	maxthresh = 0;
	for (unsigned i = 0; i < area; i++)
	{
		if (maxthresh < pf[i])
		{
			maxthresh = pf[i];
		}
	}
	if (thresh > maxthresh || thresh == 0)
		thresh = maxthresh;
	if (maxthresh == 0)
		maxthresh = thresh = 1;
	sl_thresh->setValue(min(int(thresh * 100 / maxthresh), 100));

	return;
}

QSize IFTrg_widget::sizeHint() const { return vbox1->sizeHint(); }

void IFTrg_widget::removemarks(Point p)
{
	if (bmphand->del_vm(p, 3))
	{
		common::DataSelection dataSelection;
		dataSelection.sliceNr = handler3D->get_activeslice();
		dataSelection.work = true;
		dataSelection.vvm = true;
		emit begin_datachange(dataSelection, this);

		vector<vector<mark>> *vvm = bmphand->return_vvm();
		vm.clear();
		for (vector<vector<mark>>::iterator it = vvm->begin(); it != vvm->end();
				 it++)
		{
			vm.insert(vm.end(), it->begin(), it->end());
			;
		}

		unsigned width = (unsigned)bmphand->return_width();
		for (unsigned i = 0; i < area; i++)
			lbmap[i] = 0;
		for (vector<mark>::iterator it = vm.begin(); it != vm.end(); it++)
		{
			lbmap[width * it->p.py + it->p.px] = (float)it->mark;
		}

		emit vm_changed(&vm);
		execute();

		emit end_datachange(this);
	}
}

void IFTrg_widget::slider_pressed()
{
	common::DataSelection dataSelection;
	dataSelection.sliceNr = handler3D->get_activeslice();
	dataSelection.work = true;
	emit begin_datachange(dataSelection, this);
}

void IFTrg_widget::slider_released() { emit end_datachange(this); }

FILE *IFTrg_widget::SaveParams(FILE *fp, int version)
{
	if (version >= 2)
	{
		int dummy;
		dummy = sl_thresh->value();
		fwrite(&(dummy), 1, sizeof(int), fp);
		fwrite(&thresh, 1, sizeof(float), fp);
		fwrite(&maxthresh, 1, sizeof(float), fp);
	}

	return fp;
}

FILE *IFTrg_widget::LoadParams(FILE *fp, int version)
{
	if (version >= 2)
	{
		QObject::disconnect(sl_thresh, SIGNAL(sliderMoved(int)), this,
												SLOT(slider_changed(int)));

		int dummy;
		fread(&dummy, sizeof(int), 1, fp);
		sl_thresh->setValue(dummy);
		fread(&thresh, sizeof(float), 1, fp);
		fread(&maxthresh, sizeof(float), 1, fp);

		QObject::connect(sl_thresh, SIGNAL(sliderMoved(int)), this,
										 SLOT(slider_changed(int)));
	}
	return fp;
}

void IFTrg_widget::hideparams_changed()
{
	if (hideparams)
	{
		sl_thresh->hide();
	}
	else
	{
		sl_thresh->show();
	}
}