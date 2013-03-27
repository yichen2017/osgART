/* -*-c++-*-
 *
 * osgART - ARToolKit for OpenSceneGraph
 * Copyright (C) 2005-2008 Human Interface Technology Laboratory New Zealand
 *
 * This file is part of osgART 2.0
 *
 * osgART 2.0 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * osgART 2.0 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with osgART 2.0.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <osg/Version>
#include <osg/Node>
#include <osg/NodeVisitor>


#include <osgART/Foundation>
#include <osgART/VideoLayer>
#include <osgART/PluginManager>
#include <osgART/VideoGeode>
#include <osgART/Utils>
#include <osgART/GeometryUtils>
#include <osgART/TrackerUtils>
#include <osgART/TargetCallback>
#include <osgART/TransformFilterCallback>
#include <osgART/ImageStreamCallback>
#include <osgART/VideoUtils>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include <osgDB/FileUtils>

class StatsCallback : public osg::NodeCallback
{
	osg::ref_ptr<osg::Stats> _stats;

public:

	StatsCallback(osg::Stats* stats) : _stats(stats) {}

	virtual void operator()(osg::Node* node, osg::NodeVisitor* nv)
	{
		if (_stats.valid())
		{
			_stats->report(std::cout);
		}
	}
};

int main(int argc, char* argv[])  {

	// create a root node
	osg::ref_ptr<osg::Group> root = new osg::Group;

	osgViewer::Viewer viewer;

	// attach root node to the viewer
	viewer.setSceneData(root.get());

	// add relevant handlers to the viewer
	viewer.addEventHandler(new osgViewer::StatsHandler);
	viewer.addEventHandler(new osgViewer::WindowSizeHandler);
	viewer.addEventHandler(new osgViewer::ThreadingHandler);
	viewer.addEventHandler(new osgViewer::HelpHandler);


	// preload the video and tracker
	osgART::PluginManager::instance()->load("osgart_video_dummyvideo");
	osgART::PluginManager::instance()->load("osgart_tracker_dummytracker");

	// Load a video plugin.
	osg::ref_ptr<osgART::Video> video =
		dynamic_cast<osgART::Video*>(osgART::PluginManager::instance()->get("osgart_video_dummyvideo"));

	// check if an instance of the video stream could be started
	if (!video.valid())
	{
		// Without video an AR application can not work. Quit if none found.
		osg::notify(osg::FATAL) << "Could not initialize video plugin!" << std::endl;
		exit(-1);
	}

	// found video - configure now
	osgART::VideoConfiguration* _configvideo = video->getConfiguration();

	// if the configuration is existing
	if (_configvideo)
	{
		// it is possible to configure the plugin before opening it
		_configvideo->config="Data/dummyvideo/dummyvideo.png";
	}

	//you can also configure the plugin using fields
	//before/after open/start in function of the specific field variable/function
	video->getField < osgART::TypedField<bool>* >("flip_vertical")->set(true);	

	// Open the video. This will not yet start the video stream but will
	// get information about the format of the video which is essential
	// for connecting a tracker
	// Note: configuration should be defined before opening the video
	video->open();

	osg::ref_ptr<osgART::Tracker> tracker =
		dynamic_cast<osgART::Tracker*>(osgART::PluginManager::instance()->get("osgart_tracker_dummytracker"));

	if (!tracker.valid())
	{
		// Without tracker an AR application can not work. Quit if none found.
		osg::notify(osg::FATAL) << "Could not initialize tracker plugin!" << std::endl;
		exit(-1);
	}

	// found tracker - configure now
	osgART::TrackerConfiguration* _configtracker = tracker->getConfiguration();

	// if the configuration is existing
	if (_configtracker)
	{
		// it is possible to configure the plugin before opening it
		_configtracker->config="mode=0;";
	}

	// get the tracker calibration object
	osg::ref_ptr<osgART::Calibration> calibration = tracker->getOrCreateCalibration();
	calibration->load("");

	// create a stats collector
	osg::ref_ptr<osg::Stats> myStats = new osg::Stats("osgART timings");

	// attach stats collector to the tracker
	tracker->setStats(myStats.get());

	// attach stats collector to the video
	video->setStats(myStats.get());

	// create reporting callback
	root->setUpdateCallback(new StatsCallback(myStats.get()));

	// set the image source for the tracker
	tracker->setImage(video.get());

	osgART::addEventCallback(root.get(), new osgART::TrackerCallback(tracker.get()));

	if (osg::ImageStream* imagestream = dynamic_cast<osg::ImageStream*>(video.get())) {
		osgART::addEventCallback(root.get(), new osgART::ImageStreamCallback(imagestream));
	}


	osg::ref_ptr<osgART::Target> target = tracker->addTarget("single;data/patt.hiro;80;0;0");
	if (!target.valid())
	{
		// Without target an AR application can not work. Quit if none found.
		osg::notify(osg::FATAL) << "Could not add target!" << std::endl;
		exit(-1);
	}

	target->setActive(true);

	osg::ref_ptr<osg::MatrixTransform> arTransform = new osg::MatrixTransform();

	osgART::attachDefaultEventCallbacks(arTransform.get(),target.get());

	arTransform->addChild(osgART::testCube(8));

	arTransform->getOrCreateStateSet()->setRenderBinDetails(100, "RenderBin");

	osg::ref_ptr<osg::Group> videoBackground = osgART::createBasicVideoBackground(video.get());
	videoBackground->getOrCreateStateSet()->setRenderBinDetails(0, "RenderBin");

	osg::ref_ptr<osg::Camera> cam = osgART::createBasicCamera(calibration);

	cam->addChild(arTransform.get());
	cam->addChild(videoBackground.get());

	root->addChild(cam.get());

	//for the demo we activate notification level to debug
	//to see log of video call
	osg::setNotifyLevel(osg::DEBUG_INFO);

	//BOOTSTRAP INIT
	viewer.setSceneData(root.get());

	viewer.realize();

	video->start();

	tracker->start();

	while (!viewer.done()) {
		viewer.frame();
	}

}