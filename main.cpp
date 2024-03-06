#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <libdrm/drm.h>
#include <libdrm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <QElapsedTimer>
#include <QDebug>
#include <QImage>

QImage drm_screenshot(const char *name, int crtc_index)
{
    int fd = ::open(name, O_RDWR);
    if (fd == -1)
    {
        return {};
    }
    auto _drmModeResContent = drmModeGetResources(fd);

    if (crtc_index < 0 || crtc_index >= _drmModeResContent->count_crtcs)
    {
        close(fd);
        return {};
    }
    auto _drmModeCrtcContent = drmModeGetCrtc(fd, _drmModeResContent->crtcs[crtc_index]);

    if (_drmModeCrtcContent->buffer_id == 0) // does not have fb
    {
        close(fd);
        return {};
    }
    auto _drmModeFBContent = drmModeGetFB(fd, _drmModeCrtcContent->buffer_id);

    if (_drmModeFBContent->depth != 24)
    {
        close(fd);
        return {};
    }

    if (_drmModeFBContent->handle == 0)
    {
        close(fd);
        return {};
    }

    QImage image(_drmModeFBContent->width, _drmModeFBContent->height, QImage::Format_RGB32);

    drm_mode_map_dumb mreq;
    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = _drmModeFBContent->handle;
    auto err = drmIoctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq);
    if (err)
    {
    	close(fd);
        return {};
    }

    auto size = _drmModeFBContent->pitch * _drmModeFBContent->height;

    auto data = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, mreq.offset);
    if (data == MAP_FAILED)
    {
    	close(fd);
        return {};
    }

    // screenshot
    {
        auto pitch = _drmModeFBContent->pitch;

        for (uint y = 0; y < _drmModeFBContent->height; ++y)
        {
            uint offset = y * pitch;

            auto m = reinterpret_cast<const char *>(data) + offset;
            auto pixels = reinterpret_cast<const uint *>(m);

            memcpy(image.bits() + y * _drmModeFBContent->width * sizeof (uint), pixels, _drmModeFBContent->width * sizeof (uint));
        }
    }

    close(fd);
    munmap(data, size);

    return image;
}

int main(int argc, char *argv[])
{
    QElapsedTimer timer;
    timer.start();
    
    auto image = drm_screenshot(argc >= 3 ? argv[2] : "/dev/dri/card0", 0);

    if (!image.isNull())
    {
        qDebug() << "Generated in" << timer.elapsed();
        image.save(argc >= 2 ? argv[1] : "DrmScreenshot.jpg");
        return 0;
    }

    return 1;
}
