/*Copyright (C) 2021 Wayne Mogg All rights reserved.
 *
 * This file may be used either under the terms of:
 *
 * 1. The GNU General Public License version 3 or higher, as published by
 * the Free Software Foundation, or
 *
 * This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 * ________________________________________________________________________
 *
 * Author:        Wayne Mogg
 * Date:          January 2021
 * ________________________________________________________________________
 *
 */
#include "pybind11/pybind11.h"
#include "wmodpy_survey.h"

#include "bufstring.h"
#include "dirlist.h"
#include "file.h"
#include "filepath.h"
#include "genc.h"
#include "iodir.h"
#include "ioobj.h"
#include "iodirentry.h"
#include "ioman.h"
#include "iopar.h"
#include "multiid.h"
#include "oddirs.h"
#include "oscommand.h"
#include "ptrman.h"
#include "survinfo.h"
#include "wellman.h"


namespace py = pybind11;

void init_wmodpy_survey(py::module_& m) {
    m.def("get_surveys", [](const char* basedir) {
	py::list list;
	if (!IOMan::isValidDataRoot(basedir))
	    return list;
	DirList dl(basedir, File::DirsInDir);
	for (int idx=0; idx<dl.size(); idx++) {
	    const BufferString& dirnm = dl.get(idx);
	    const FilePath fp(basedir, dirnm);
	    const BufferString fpstr = fp.fullPath();
	    if (File::exists(fpstr) && IOMan::isValidSurveyDir(fpstr))
		list.append(std::string(dirnm));
	}
	return list;
    }, "Return list of survey names in given survey data root",
	py::arg("survey_data_root"));

    py::class_<wmSurvey>(m, "Survey", "Encapsulates an OpendTect survey")
	.def(py::init<const std::string&, const std::string&>())
	.def("name", &wmSurvey::name, "Return the survey name")
	.def("isok", &wmSurvey::isOK, "Return if the survey is properly setup and accessible")
	.def("has2d", &wmSurvey::has2D, "Return if the survey contains 2D seismic data")
	.def("has3d", &wmSurvey::has3D, "Return if the the survey contains 3D seismic data")
	.def("epsg", &wmSurvey::epsgCode, "Return the survey CRS EPSG code");

}

wmSurvey::wmSurvey(const std::string& basedir, const std::string& surveynm)
    : basedir_(basedir)
    , survey_(surveynm)
{
    activate();
    FilePath fp(basedir.c_str(), surveynm.c_str());
    const BufferString fpstr = fp.fullPath();
    if (File::exists(fpstr))
	si_ = SurveyInfo::read(fpstr);
}

wmSurvey::~wmSurvey()
{
    if (si_) delete si_;
}

std::string wmSurvey::name() const
{
    return si_ ? std::string(si_->name()) : std::string();
}

bool wmSurvey::isOK() const
{
    return si_;
}

bool wmSurvey::has2D() const
{
    return si_ ? si_->has2D() : false;
}

bool wmSurvey::has3D() const
{
    return si_ ? si_->has3D() : false;
}

std::string wmSurvey::epsgCode() const
{
    BufferString epsg(si_->pars().find("Coordinate System.Projection.ID"));
    epsg.replace("`", ":");
    return std::string(epsg);
}

std::string wmSurvey::surveyPath() const
{
    FilePath fp(si_->getDataDirName(), si_->getDirName());
    return std::string(fp.fullPath());
}

void wmSurvey::activate() const {
    OS::MachineCommand mc;
    mc.addArg("/opt/seismic/OpendTect_6/6.6.0/bin/lux64/Release/od_main");
    mc.addKeyedArg("dtectdata", basedir_.c_str());
    mc.addKeyedArg("survey", survey_.c_str());
    int argc = mc.args().size();
    char** argv = new char*[argc+1];
    for (int idx=0; idx<argc; idx++) {
	BufferString* arg = new BufferString(mc.args().get(idx));
	argv[idx] = arg->getCStr();
    }
    argv[argc] = 0;
    SetProgramArgs( argc, argv);
    IOM().setDataSource(basedir_.c_str(), survey_.c_str(), true);
    Well::MGR().cleanup();
}