#include <cerrno>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include "PPS.hxx"

// PPS access needs root rights
// you can load the kernel module "pps-ktimer" to get a PPS source to play with

using ShDevice = std::shared_ptr<PPS::Device>;
static const std::string DefaultDevice("/dev/pps0");

int32_t prepare(ShDevice pps_source, struct pps_ktime &offset_assert, int &supported_modes) noexcept
{
    struct pps_kparams params;

    if (!pps_source->valid())
    {
        std::cerr << "error: device " << pps_source->deviceName() << " is not accessable"
                  << std::endl;
        return false;
    }
    else
        std::cerr << "device: " << pps_source->deviceName() << " (working)" << std::endl;

    if (pps_source->caps(supported_modes))
    {
        if (!(supported_modes & PPS_CAPTUREASSERT))
        {
            std::cerr << "modes: PPS_CAPTUREASSERT (notsupported)" << std::endl;
            return false;
        }
        else
            std::cout << "modes: PPS_CAPTUREASSERT (supported)" << std::endl;
    }
    else
    {
        std::cerr << "error: PPS_CAPTUREASSERT query failed (" << pps_source->error() << ')'
                  << std::endl;
        return false;
    }

    if (!pps_source->parameters(params))
    {
        std::cerr << "error: unable to query parameters (" << pps_source->error() << ')'
                  << std::endl;
        return false;
    }

    params.mode |= PPS_CAPTUREASSERT;
    if (supported_modes & PPS_OFFSETASSERT)
    {
        params.mode |= PPS_OFFSETASSERT;
        params.assert_off_tu = offset_assert;
    }

    if (!pps_source->setParameters(params))
    {
        std::cerr << "error: unable to set parameters (" << pps_source->error() << ')' << std::endl;
        return false;
    }

    return true;
}

bool print(ShDevice pps_source, int32_t &supported_modes) noexcept
{
    struct pps_fdata data;
    struct timespec timeout = {3, 0};
    int32_t err = 0;

    while (true)
    {
        if (supported_modes & PPS_CANWAIT)
        {
            err = pps_source->fetch(data, timeout);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            err = pps_source->fetch(data, timeout);
        }

        if (err < 0)
        {
            if (err == -EINTR)
            {
                std::cerr << "warn: fetch() recieved INTR signal" << std::endl;
                continue;
            }

            std::cerr << "error: fetch() error " << err << '(' << strerror(errno) << ')'
                      << std::endl;
            return false;
        }

        break;
    }

    std::cout << "device " << pps_source->deviceName()
              << " - assert " << std::setw(10) << std::setfill('0') << data.info.assert_tu.sec << '.'
                  << std::setw(9) << std::setfill('0') << data.info.assert_tu.nsec
                  << " - sequence " << data.info.assert_sequence
              << " - clear " << std::setw(10) << std::setfill('0') << data.info.clear_tu.sec << '.'
                  << std::setw(9) << std::setfill('0') << data.info.clear_tu.nsec
                  << " - sequence " << data.info.clear_sequence
              << std::endl;

    return true;
}

void usage(const std::string &appname) noexcept
{
    std::cout << "usage: " << appname << "<option>\n"
              << "options:\n"
              << "  --help          show this help screen\n"
              << "  --device=<dev>  path to PPS device (default: " << DefaultDevice << ")\n"
              << std::endl;
}

int32_t main(int32_t argc, char **argv) noexcept
{
    std::string devname = DefaultDevice;
    ShDevice pps;
    struct pps_ktime offset = {0, 0, 0};
    int32_t modes = 0;

    for (int32_t i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);

        if (arg == "--help")
        {
            usage(argv[0]);
            return 0;
        }

        if ((arg.size() > 9) && (arg.substr(0, 9) == "--device="))
        {
            devname = arg.substr(9, std::string::npos);
            continue;
        }
    }

    try
    {
        pps = std::make_shared<PPS::Device>(devname);
    }
    catch (std::exception &e)
    {
        std::cerr << "error: " << e.what() << std::endl;
        return 1;
    }

    if (!prepare(pps, offset, modes))
        return 1;

    while (true)
    {
        if (!print(pps, modes))
            return 1;
    }

    return 0;
}
