#pragma once

#include <string>
#include <linux/pps.h>

namespace PPS
{
    class Device {
    public:
        //--- public constructors ---
        Device(const std::string &devname = "/dev/pps0") noexcept(false);
        Device(const Device &rhs) = delete;
        Device(Device &&rhs) = delete;
        virtual ~Device() noexcept;

        //--- public operators ---
        Device &operator=(const Device &rhs) = delete;
        Device &operator=(Device &&rhs) = delete;

        //--- public methods ---
        bool valid() const noexcept;
        std::string error() noexcept(false);
        std::string deviceName() const noexcept;

        bool parameters(struct pps_kparams &params) noexcept;
        bool setParameters(const struct pps_kparams &params) noexcept;
        bool caps(int32_t &mode) noexcept;
        bool fetch(struct pps_fdata &fdata, const struct timespec &timeout) noexcept;

    protected:
        //--- protected methods ---
        bool open() noexcept;
        bool close() noexcept;

    private:
        //--- private properties ---
        std::string _devname;
        int32_t _fd;    // non-negative = okay, -1 = not okay
        int32_t _err;   // 0 = not set/unused, positive number = errno set
    };
}
