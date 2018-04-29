// //////////////////////////////////////////////////////////////////////////
// IceWM: src/pppstatus.h
//
//
// PPP Status
//
// //////////////////////////////////////////////////////////////////////////
#ifndef NETSTATUS_H
#define NETSTATUS_H

#ifndef HAVE_NET_STATUS
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || \
    defined(__OpenBSD__) || defined(__FreeBSD_kernel__)
#define HAVE_NET_STATUS 1
#endif
#endif

#if HAVE_NET_STATUS

#include "ypointer.h"

class IAppletContainer;
class NetStatusControl;

typedef unsigned long long netbytes;

class NetDevice {
public:
    NetDevice(cstring netdev) : fDevName(netdev) {}
    virtual bool isUp() = 0;
    virtual void getCurrent(netbytes *in, netbytes *out, const void* sharedData) = 0;
    virtual const char* getPhoneNumber() { return ""; }
    virtual ~NetDevice() {}
protected:
    cstring const fDevName;
};

class NetLinuxDevice : public NetDevice {
public:
    NetLinuxDevice(cstring netdev) : NetDevice(netdev) {}
    virtual bool isUp();
    virtual void getCurrent(netbytes *in, netbytes *out, const void* sharedData);
};

class NetIsdnDevice : public NetLinuxDevice {
public:
    NetIsdnDevice(cstring netdev) : NetLinuxDevice(netdev) { *phoneNumber = 0; }
    virtual bool isUp();
    virtual const char* getPhoneNumber() { return phoneNumber; }
private:
    char phoneNumber[32];
};

class NetFreeDevice : public NetDevice {
public:
    NetFreeDevice(cstring netdev) : NetDevice(netdev) {}
    virtual bool isUp();
    virtual void getCurrent(netbytes *in, netbytes *out, const void* sharedData);
};

class NetOpenDevice : public NetDevice {
public:
    NetOpenDevice(cstring netdev) : NetDevice(netdev) {}
    virtual bool isUp();
    virtual void getCurrent(netbytes *in, netbytes *out, const void* sharedData);
};

class NetStatus: public YWindow {
public:
    NetStatus(IApp *app, YSMListener *smActionListener, cstring netdev, IAppletContainer *taskBar, YWindow *aParent = 0);
    ~NetStatus();

    cstring name() const { return fDevName; }
    void timedUpdate(const void* sharedData, bool forceDown = false);

private:
    IAppletContainer *fTaskBar;
    YColorName color[3];
    YSMListener *smActionListener;
    IApp *app;

    long *ppp_in; /* long could be really enough for rate in B/s */
    long *ppp_out;

    netbytes prev_ibytes, start_ibytes, cur_ibytes, offset_ibytes;
    netbytes prev_obytes, start_obytes, cur_obytes, offset_obytes;

    timeval start_time;
    timeval prev_time;

    Drawable pixmap;
    long oldMaxBytes;

    bool wasUp;               // previous link status
    bool useIsdn;             // netdevice is an IsdnDevice
    cstring const fDevName;   // name of the device
    osmart<NetDevice> fDevice;

    void updateVisible(bool aVisible);
    // methods local to this class
    bool isUp() const { return fDevice && fDevice->isUp(); }
    void getCurrent(long *in, long *out, const void* sharedData);
    void updateStatus(const void* sharedData);
    void updateToolTip();

    // methods overridden from superclasses
    virtual void handleClick(const XButtonEvent &up, int count) OVERRIDE;
    virtual void paint(Graphics & g, const YRect &r) OVERRIDE;

    void picture();
    void fill(Graphics& g);
    void draw(Graphics& g);
};

#ifdef __linux__
class netpair : public pair<const char *, const char *> {
public:
    netpair(const char* name, const char* data) : pair(name, data) { }
    const char* name() const { return left; }
    const char* data() const { return right; }
};
#endif

class NetStatusControl : public YTimerListener, public refcounted {
private:
    lazy<YTimer> fUpdateTimer;
    YAssocArray<NetStatus*> fNetStatus;

    IApp* app;
    YSMListener* smActionListener;
    IAppletContainer* taskBar;
    YWindow* aParent;

#ifdef __linux__
    // preprocessed data from procfs with offset table (name, values, name, vaues, ...)
    csmart devicesText;
    YArray<netpair> devStats;
    typedef YArray<netpair>::IterType IterStats;

    void fetchSystemData();
    void linuxUpdate();
#endif
    YStringArray patterns;
    NetStatus* createNetStatus(cstring netdev);
    void getInterfaces(YStringArray& interfaces);

public:
    NetStatusControl(IApp *app, YSMListener *smActionListener, IAppletContainer *taskBar, YWindow *aParent);
    ~NetStatusControl();

    typedef YAssocArray<NetStatus*>::IterType IterType;
    IterType getIterator() { return fNetStatus.iterator(); }

    // subclassing method overrides
    virtual bool handleTimer(YTimer *t) OVERRIDE;
};

#endif

#endif // NETSTATUS_H

// vim: set sw=4 ts=4 et:
