/*
 * This file is part of the OpenKinect Project. http://www.openkinect.org
 *
 * Copyright (c) 2010 individual OpenKinect contributors. See the CONTRIB file
 * for details.
 *
 * This code is licensed to you under the terms of the Apache License, version
 * 2.0, or, at your option, the terms of the GNU General Public License,
 * version 2.0. See the APACHE20 and GPL2 files for the text of the licenses,
 * or the following URLs:
 * http://www.apache.org/licenses/LICENSE-2.0
 * http://www.gnu.org/licenses/gpl-2.0.txt
 *
 * If you redistribute this file in source form, modified or unmodified, you
 * may:
 *   1) Leave this header intact and distribute it under the same terms,
 *      accompanying it with the APACHE20 and GPL20 files, or
 *   2) Delete the Apache 2.0 clause and accompany it with the GPL2 file, or
 *   3) Delete the GPL v2 clause and accompany it with the APACHE20 file
 * In all cases you must keep the copyright notice intact and include a copy
 * of the CONTRIB file.
 *
 * Binary distributions must follow the binary distribution requirements of
 * either License.
 */

#pragma once

#include <libfreenect.h>
#include <stdexcept>
#include <map>
#include <pthread.h>

namespace Freenect {
	class Noncopyable {
	  public:
		Noncopyable() {}
		~Noncopyable() {}
	  private:
		Noncopyable( const Noncopyable& );
		const Noncopyable& operator=( const Noncopyable& );
	};

	class FreenectDevice : Noncopyable {
	  public:
		FreenectDevice(freenect_context *_ctx, int _index) {
			if(freenect_open_device(_ctx, &m_dev, _index) != 0) throw std::runtime_error("Cannot open Kinect");
			freenect_set_user(m_dev, this);
			freenect_set_rgb_format(m_dev, FREENECT_FORMAT_RGB);
			freenect_set_depth_format(m_dev, FREENECT_FORMAT_11_BIT);
			freenect_set_depth_callback(m_dev, freenect_depth_callback);
			freenect_set_rgb_callback(m_dev, freenect_rgb_callback);
		}
		~FreenectDevice() {
			if(freenect_close_device(m_dev) != 0) throw std::runtime_error("Cannot shutdown Kinect");
		}
		void startRGB() {
			if(freenect_start_rgb(m_dev) != 0) throw std::runtime_error("Cannot start RGB callback");
		}
		void stopRGB() {
			if(freenect_stop_rgb(m_dev) != 0) throw std::runtime_error("Cannot stop RGB callback");
		}
		void startDepth() {
			if(freenect_start_depth(m_dev) != 0) throw std::runtime_error("Cannot start depth callback");
		}
		void stopDepth() {
			if(freenect_stop_depth(m_dev) != 0) throw std::runtime_error("Cannot stop depth callback");
		}
		void setTiltDegrees(double _angle) {
			if(freenect_set_tilt_degs(m_dev, _angle) != 0) throw std::runtime_error("Cannot set angle in degrees");
		}
		void setLed(freenect_led_options _option) {
			if(freenect_set_led(m_dev, _option) != 0) throw std::runtime_error("Cannot set led");
		}
		void getAccelerometers(double &_x, double &_y, double &_z) {
			if(freenect_get_mks_accel(m_dev,&_x, &_y, &_z) != 0) throw std::runtime_error("Cannot get mks accemerometers");
		}
		void getAccelerometers(int16_t &_x, int16_t &_y, int16_t &_z) {
			if(freenect_get_raw_accel(m_dev,&_x, &_y, &_z) != 0) throw std::runtime_error("Cannot get raw accemerometers");
		}
		// Do not call directly even in child
		virtual void RGBCallback(freenect_pixel *rgb, uint32_t timestamp) = 0;
		// Do not call directly even in child
		virtual void DepthCallback(freenect_depth *depth, uint32_t timestamp) = 0;
	  private:
		freenect_device *m_dev;
		static void freenect_depth_callback(freenect_device *dev, freenect_depth *depth, uint32_t timestamp) {
			FreenectDevice* device = static_cast<FreenectDevice*>(freenect_get_user(dev));
			device->DepthCallback(depth, timestamp);
		}
		static void freenect_rgb_callback(freenect_device *dev, freenect_pixel *rgb, uint32_t timestamp) {
			FreenectDevice* device = static_cast<FreenectDevice*>(freenect_get_user(dev));
			device->RGBCallback(rgb, timestamp);
		}
	};

	template <class T>class Freenect : Noncopyable {
	  public:
		Freenect() : m_stop(false) {
			if(freenect_init(&m_ctx, NULL) != 0) throw std::runtime_error("Cannot initialize freenect library");
			if(pthread_create(&m_thread, NULL, pthread_callback, (void*)this) != 0) throw std::runtime_error("Cannot initialize freenect thread");
		}
		~Freenect() {
			for(typename std::map<int, T*>::iterator it = m_devices.begin() ; it != m_devices.end() ; ++it) {
				delete it->second;
			}
			m_stop = true;
			pthread_join(m_thread, NULL);
			if(freenect_shutdown(m_ctx) != 0) throw std::runtime_error("Cannot cleanup freenect library");
		}
		T& createDevice(int _index) {
			m_devices.insert(std::make_pair<int, T*>(_index, new T(m_ctx, _index)));
			return *(m_devices.at(_index));
		}
		void deleteDevice(int _index) {
			m_devices.erase(_index);
		}
		int deviceCount() {
			return freenect_num_devices(m_ctx);
		}
		// Do not call directly, thread runs here
		void operator()() {
			while(!m_stop) {
				if(freenect_process_events(m_ctx) != 0) throw std::runtime_error("Cannot process freenect events");
			}
		}
		static void *pthread_callback(void *user_data) {
			Freenect<T>* freenect = static_cast<Freenect<T>*>(user_data);
			(*freenect)();
		}
	  private:
		freenect_context *m_ctx;
		volatile bool m_stop;
		pthread_t m_thread;
		std::map<int, T*> m_devices;
	};

}

