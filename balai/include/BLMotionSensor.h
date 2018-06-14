//
//  MotionSensor.h
//
//  Created by andre.hl.chen@gmail.com on 12/8/29.
//
#ifndef MOTION_SENSOR_H
#define MOTION_SENSOR_H

#include "BLQuaternion.h"

namespace mlabs { namespace balai { namespace input {

// hardware sensor
enum SENSOR_TYPE {
    TYPE_ACCELEROMETER  = 0,    // m/s^2
    TYPE_GYROSCOPE      = 1,    // rad/s
    TYPE_MAGNETIC_FIELD = 2,    // microtesla, magnetic flux density
                                // fast facts:strength of Earth's magnetic field on the equator = 31 microtesla)
    TYPE_SAMPLE_TOTALS  = 3,

    // this could be Hardware or Software, so we don't log all samples
    TYPE_ROTATION_VECTOR,
};

class SensorManager
{
    enum { SAMPLE_SIZE = 60 };

    // sample data
    math::Vector3 samples_[TYPE_SAMPLE_TOTALS][SAMPLE_SIZE];

    // rotation vector, i.e. x,y,z of a quaternion
    math::Vector3 rv_[2]; // double buffering

    float  sampleTimeStart_[TYPE_SAMPLE_TOTALS];
    float  sampleFPS_[TYPE_SAMPLE_TOTALS];
    uint32 sampleIndex_[TYPE_SAMPLE_TOTALS];
    float  rvSampleTimeStart_;
    float  rvSampleFPS_;
    uint32 rvSampleIndex_;
    bool   debugLog_;

    BL_NO_COPY_ALLOW(SensorManager);

    // private
    SensorManager():
        rvSampleTimeStart_(0.0f),rvSampleFPS_(0.0f),rvSampleIndex_(0),debugLog_(false) {
        for (int i=0; i<TYPE_SAMPLE_TOTALS; ++i) {
            for (int j=0; j<SAMPLE_SIZE; ++j) {
                samples_[i][j].Zero();
            }
            sampleTimeStart_[i] = 0.0f;
            sampleFPS_[i] = 0.0f;
            sampleIndex_[i] = 0;
        }
        rv_[0].Zero();
        rv_[1].Zero();
    }
    
    static math::Quaternion const deviceOffset_portrait;
    static math::Quaternion const deviceOffset_landscape;

public:
    ~SensorManager() {}

    // debug log
    bool DebugLogEnable() const { return debugLog_; }
    void DebugLog(bool enable) { debugLog_ = enable; }
    void ToggleDebugLog() { debugLog_ = !debugLog_; }

    // draw
    void drawSensorData(SENSOR_TYPE what, float left, float top, float right, float bottom) const;

    void AddSensorData(SENSOR_TYPE what, float x, float y, float z) {
        if (what<TYPE_SAMPLE_TOTALS) {
            uint32& index = sampleIndex_[what];
            math::Vector3& sample = samples_[what][index];
            if (what==TYPE_ACCELEROMETER) {
                // simple low-pass filter...
                //float const a = 0.1f;
                //float const b = 1.0f - a;
                //math::Vector3& prev = samples_[TYPE_ACCELEROMETER][(index+SAMPLE_SIZE-1)%SAMPLE_SIZE];
                //sample.x = b*prev.x + a*x; 
                //sample.y = b*prev.y + a*y; 
                //sample.z = b*prev.z + a*z;
                sample.x = x; sample.y = y; sample.z = z;
            }
            else {
                sample.x = x; sample.y = y; sample.z = z;
            }

            if (++index>=SAMPLE_SIZE) {
                index = 0;
                float const time = (float) system::GetTime();
                sampleFPS_[what] = SAMPLE_SIZE/(time - sampleTimeStart_[what]);
                sampleTimeStart_[what] = time;
            }         

            if (debugLog_) {
                if (TYPE_ACCELEROMETER==what) {
                    BL_LOG("MOTION_A:%f,%f,%f", x, y, z);
                }
                else if (TYPE_GYROSCOPE==what) {
                    BL_LOG("MOTION_G:%f,%f,%f",x,y,z);
                }
                else if (TYPE_MAGNETIC_FIELD==what) {
                    BL_LOG("MOTION_C:%f,%f,%f",x,y,z);
                }
            }
        }
        else if (TYPE_ROTATION_VECTOR==what) {
            math::Vector3& rv = rv_[rvSampleIndex_%2];
            rv.x = x; rv.y = y; rv.z = z;
            if (++rvSampleIndex_>=SAMPLE_SIZE) {
                rvSampleIndex_ = 0;
                float const time = (float) system::GetTime();
                rvSampleFPS_ = SAMPLE_SIZE/(time - rvSampleTimeStart_);
                rvSampleTimeStart_ = time;
            }
        }
    }

    float GetMinMax(SENSOR_TYPE what, math::Vector3& min, math::Vector3& max) const {
        if (what<TYPE_SAMPLE_TOTALS) {
            math::Vector3 const* sample = samples_[what];
            min = max = sample[0];
            float mag = min.NormSq();
            for (int i=1; i<SAMPLE_SIZE; ++i) {
                math::Vector3 const& v = sample[i];
                if (min.x>v.x) min.x = v.x;
                if (min.y>v.y) min.y = v.y;
                if (min.z>v.z) min.z = v.z;

                if (max.x<v.x) max.x = v.x;
                if (max.y<v.y) max.y = v.y;
                if (max.z<v.z) max.z = v.z;
                float const t = v.NormSq();
                if (t>mag)
                    mag = t;
            }
            return math::Sqrt(mag);
        }
        return -1.0f;
    }

    math::Vector3 const& GetSensorData(SENSOR_TYPE what) const {
        if (what<TYPE_SAMPLE_TOTALS) {
            return samples_[what][(sampleIndex_[what]+(SAMPLE_SIZE-1))%SAMPLE_SIZE];
		}
        else {
            return rv_[(rvSampleIndex_+1)%2];
        }
    }

    math::Vector3 const* GetSamples(SENSOR_TYPE what, uint32& start, uint32& totalSize) const {
        if (what<TYPE_SAMPLE_TOTALS) {
            start = sampleIndex_[what];
            totalSize = (uint32) SAMPLE_SIZE;
            return samples_[what];
        }
        return NULL;
    }

    float GetSampleRate(SENSOR_TYPE what) const {
        return (what<TYPE_SAMPLE_TOTALS) ? sampleFPS_[what]:rvSampleFPS_;
    }

    math::Quaternion const GetOrientation(bool isLandscape) const {
        math::Quaternion const deviceOffset = isLandscape ? deviceOffset_landscape:deviceOffset_portrait;
        math::Vector3 const& rv = rv_[(rvSampleIndex_+1)%2];
        return math::Quaternion(math::Sqrt(1.0f - rv.NormSq()), rv.x, rv.y, rv.z) * deviceOffset;
    }

    // singleton
    static SensorManager& GetInstance() {
        static SensorManager inst_;
        return inst_;
    }
};


enum UI_ORIENTATION {
    UI_ORIENTATION_PORTRAIT         = 0, // portrait
    UI_ORIENTATION_LANDSCAPE_CCW90  = 1, // lanscape, home bottom to the right
    UI_ORIENTATION_PORTRAIT_180     = 2, // portrait, upside down
    UI_ORIENTATION_LANDSCAPE_CCW270 = 3  // landscape, home bottom to the left
};

// accelerometer + gyroscope sensors fusion
class SensorFusion
{
    mutable system::Mutex mutex_;
    math::Vector3 gyroBuffer_[4];
    math::Vector3 accBuffer_[4];
	math::Quaternion compass_[4];
    math::Quaternion fusion_;
    UI_ORIENTATION   orientation_;

    ////////////////////////////////////////////////////
    // experient values -
    //              gyro_scale_     gyro_cutoff_
    // m7+Ivensense     1.02f           2.0f dps
    // m7+ST LSM330     1.00f           2.0f dps
    // T6+ST LSM330     1.00f           2.0f dps
    ////////////////////////////////////////////////////
    float gyro_cutoff_;
    float gyro_scale_;
    float fusion_Kp_; // proportional gain (Kp)
    volatile float stable_time_;
    volatile uint8 gyro_put_; // index to put gyro data
    volatile uint8 acc_put_;
	volatile uint8 compass_put_;
    uint8   fusionEnabled_;

    // coordinate transform : to transform sensor data(x, y, z) from android device coordinate system to balai coordinate system
    void CoordinateTransform_(math::Vector3& v, float x, float y, float z);

    // fusion step
    void FusionUpdate_(math::Vector3 gyro, math::Vector3 acc, float deltaTime);

    // declare but not define
    SensorFusion(SensorFusion const&);
    SensorFusion& operator=(SensorFusion const&);

public:
    SensorFusion():mutex_(),
        fusion_(),
        orientation_(UI_ORIENTATION_PORTRAIT),
        gyro_cutoff_(2.0f*math::constants::float_deg_to_rad),gyro_scale_(1.0f),
        fusion_Kp_(0.5f),
        stable_time_(0.0f),gyro_put_(0),acc_put_(0),compass_put_(0),fusionEnabled_(0) {
        for (int i=0; i<4; ++i) {
            gyroBuffer_[i].Zero();
            accBuffer_[i].Zero();
        }
    }

    void SetUIOrientation(UI_ORIENTATION orientation);
    void Start(UI_ORIENTATION orientation=UI_ORIENTATION_PORTRAIT) {
        SetUIOrientation(orientation);
        fusionEnabled_ = 1;
    }
    void Stop() { fusionEnabled_ = 0; }

    float GetStableTime() const { return stable_time_; }
    math::Vector3 GetGyroData() const { return gyroBuffer_[(gyro_put_+3)%4]; }
    math::Vector3 GetAccData() const { return accBuffer_[(acc_put_+3)%4]; }

    bool GetBalance(float& roll, float& pitch) const;

    void SampleAccData(float x, float y, float z, float deltaTime);
    void SampleGyroData(float x, float y, float z, float deltaTime);
	void SampleRotationVector(float x, float y, float z, float deltaTime);

    void SetGyroParameters(float cutoff, float scale=1.0f) {
        gyro_cutoff_ = cutoff*math::constants::float_deg_to_rad;
        gyro_scale_  = scale;
    }

    // sensor fusion functions
    void fusion_setparameters(float Kp/*=0.5f*/) { fusion_Kp_ = Kp; }
    math::Quaternion const& fusion_reset(float yaw=0.0f) {
        BL_MUTEX_LOCK(mutex_);
        fusion_.SetEulerAngles(0.0f, 0.0f, yaw);
        math::Vector3 acc(accBuffer_[(acc_put_+3)%4]);
        float norm = 0.0f;
        acc.Normalize(&norm);
        if (norm>1.0f) {
            math::Quaternion dQ;
            fusion_ *= dQ.SetConstraint(acc, math::Vector3(0.0f, 0.0f, 1.0f));
        }
        fusionEnabled_ = 1;
        return fusion_;
    }
    math::Quaternion const& fusion_result() const {
        BL_MUTEX_LOCK(mutex_);
        return fusion_;
    }
    math::Quaternion const fusion_result2() const {
        math::Quaternion q;
        {
            BL_MUTEX_LOCK(mutex_);
            q = fusion_;
        }
        if (stable_time_>0.1f) {
            math::Vector3 acc(accBuffer_[(acc_put_+3)%4]);
            float norm = 0.0f;
            acc.Normalize(&norm);
            if (norm>1.0f) {
                math::Quaternion dQ;
                q *= dQ.SetConstraint(acc, q.GetInverse().ZAxis());
            }
        }
        return q;
    }
};

}}} // namespace mlabs::balai::input

#endif