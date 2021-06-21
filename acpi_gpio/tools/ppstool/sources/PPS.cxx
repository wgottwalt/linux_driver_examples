#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <utility>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "PPS.hxx"

namespace PPS
{
    //--- public constructors ---

    Device::Device(const std::string &devname) noexcept(false)
    : _devname(devname), _fd(-1), _err(0)
    {
        if (!open())
            throw std::runtime_error(::strerror(errno));
    }

    Device::~Device() noexcept
    {
        close();
    }

    //--- public methods ---

    bool Device::valid() const noexcept
    {
        return (_fd > -1) && !_err;
    }

    std::string Device::error() noexcept(false)
    {
        if (_err)
        {
            const std::string tmp(strerror(_err));

            _err = 0;

            return tmp;
        }

        return "";
    }

    std::string Device::deviceName() const noexcept
    {
        return _devname;
    }

    bool Device::parameters(struct pps_kparams &params) noexcept
    {
        if (valid())
        {
            struct pps_kparams tmp_params;

            if (::ioctl(_fd, PPS_GETPARAMS, &tmp_params) > -1)
            {
                params = tmp_params;
                return true;
            }

            _err = errno;
        }

        return false;
    }

    bool Device::setParameters(const struct pps_kparams &params) noexcept
    {
        if (valid())
        {
            struct pps_kparams tmp_params = params;

            if (::ioctl(_fd, PPS_SETPARAMS, &tmp_params) > -1)
                return true;

            _err = errno;
        }

        return false;
    }

    bool Device::caps(int32_t &mode) noexcept
    {
        if (valid())
        {
            int32_t tmp;

            if (::ioctl(_fd, PPS_GETCAP, &tmp) > -1)
            {
                mode = tmp;
                return true;
            }

            _err = errno;
        }

        return false;
    }

    bool Device::fetch(struct pps_fdata &fdata, const struct timespec &timeout) noexcept
    {
        if (valid())
        {
            struct pps_fdata tmp_fdata;

            tmp_fdata.timeout.sec = timeout.tv_sec;
            tmp_fdata.timeout.nsec = timeout.tv_nsec;
            if (::ioctl(_fd, PPS_FETCH, &tmp_fdata) > -1)
            {
                fdata = tmp_fdata;
                return true;
            }

            _err = errno;
        }

        return false;
    }

    //--- protected methods ---

    bool Device::open() noexcept
    {
        const int32_t result = ::open(_devname.c_str(), O_RDWR);

        if (result > -1)
        {
            _fd = result;
            _err = 0;
        }
        else
        {
            _fd = -1;
            _err = errno;
        }

        return (_fd > -1) && !_err;
    }

    bool Device::close() noexcept
    {
        int32_t result = ::close(_fd);

        if (result == 0)
        {
            _fd = -1;
            _err = 0;
        }
        else
            _err = errno;

        return (result == 0) ? true : false;
    }
}
