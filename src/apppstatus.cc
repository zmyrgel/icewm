// //////////////////////////////////////////////////////////////////////////
// IceWM: src/NetStatus.cc
// by Mark Lawrence <nomad@null.net>
//
// Linux-ISDN/ippp-Upgrade
// by Denis Boehme <denis.boehme@gmx.net>
//     6.01.2000
//
// !!! share code with cpu status
//
// KNOWN BUGS: - first measurement is throwed off
//
// //////////////////////////////////////////////////////////////////////////

#include "config.h"
#include "wmtaskbar.h"
#include "apppstatus.h"

#ifdef HAVE_NET_STATUS

#include "wmapp.h"
#include "prefs.h"
#include "sysdep.h"
#include "intl.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <fnmatch.h>

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
#include <sys/sysctl.h>
#include <net/if_mib.h>
#endif

extern ref<YPixmap> taskbackPixmap;

NetStatus::NetStatus(
    IApp *app,
    YSMListener *smActionListener,
    cstring netdev,
    IAppletContainer *taskBar,
    YWindow *aParent):
    YWindow(aParent),
    fTaskBar(taskBar),
    smActionListener(smActionListener),
    app(app),
    ppp_in(new long[taskBarNetSamples]),
    ppp_out(new long[taskBarNetSamples]),
    prev_ibytes(0),
    start_ibytes(0),
    cur_ibytes(0),
    offset_ibytes(0),
    prev_obytes(0),
    start_obytes(0),
    cur_obytes(0),
    offset_obytes(0),
    start_time(monotime()),
    prev_time(start_time),
    wasUp(false),
    useIsdn(netdev.m_str().startsWith("ippp")),
    fDevName(netdev),
    fDevice(
#if defined(__linux__)
            useIsdn
                ? (NetDevice *) new NetIsdnDevice(netdev)
                : (NetDevice *) new NetLinuxDevice(netdev)
#elif defined(__FreeBSD__)
            new NetFreeDevice(netdev)
#elif defined(__OpenBSD__) || defined(__NetBSD__)
            new NetOpenDevice(netdev)
#else
            0
#endif
            )
{
    for (int i = 0; i < taskBarNetSamples; i++)
        ppp_in[i] = ppp_out[i] = 0;

    color[0] = &clrNetReceive;
    color[1] = &clrNetSend;
    color[2] = &clrNetIdle;

    setSize(taskBarNetSamples, taskBarGraphHeight);

    getCurrent(0, 0, 0);
    updateStatus(0);
    if (isUp()) {
        updateVisible(true);
        updateToolTip();
    }
    setTitle(cstring("NET-" + netdev));
}

NetStatus::~NetStatus() {
    delete[] ppp_in;
    delete[] ppp_out;
}

void NetStatus::updateVisible(bool aVisible) {
    if (visible() != aVisible) {
        if (aVisible)
            show();
        else
            hide();

        fTaskBar->relayout();
    }
}

void NetStatus::timedUpdate(const void* sharedData, bool forceDown) {

    bool up = !forceDown && isUp();

    if (up) {
        if (!wasUp) {
            for (int i = 0; i < taskBarNetSamples; i++)
                ppp_in[i] = ppp_out[i] = 0;

            start_time = monotime();
            cur_ibytes = 0;
            cur_obytes = 0;

            updateStatus(sharedData);
            start_ibytes = cur_ibytes;
            start_obytes = cur_obytes;
            updateVisible(true);
        }
        updateStatus(sharedData);

        if (toolTipVisible() || !wasUp)
            updateToolTip();
    }
    else // link is down
        if (wasUp)
            updateVisible(false);

    wasUp = up;
}

void NetStatus::updateToolTip() {
    char status[400];

    if (isUp()) {
        char const * const sizeUnits[] = { "B", "KiB", "MiB", "GiB", "TiB", NULL };
        char const * const rateUnits[] = { "B/s", "kB/s", "MB/s", NULL };

        long const period(long(toDouble(monotime() - start_time)));

/*      long long vi(cur_ibytes - start_ibytes);
        long long vo(cur_obytes - start_obytes); */
        long long vi(cur_ibytes);
        long long vo(cur_obytes);

        long ci(ppp_in[taskBarNetSamples - 1]);
        long co(ppp_out[taskBarNetSamples - 1]);

        /* ai and oi were keeping nonsenses (if were not reset by
         * double-click) because of bad control of start_obytes and
         * start_ibytes (these were zeros that means buggy)
         *
         * Note: as start_obytes and start_ibytes now keep right values,
         * 'Transferred' now displays amount related to 'Online time' (and not
         * related to uptime of machine as was displayed before) -stibor- */
/*      long ai(t ? vi / t : 0);
        long ao(t ? vo / t : 0); */
        long long cai = 0;
        long long cao = 0;

        for (int ii = 0; ii < taskBarNetSamples; ii++) {
            cai += ppp_in[ii];
            cao += ppp_out[ii];
        }
        cai /= taskBarNetSamples;
        cao /= taskBarNetSamples;

        const char * const viUnit(niceUnit(vi, sizeUnits));
        const char * const voUnit(niceUnit(vo, sizeUnits));
        const char * const ciUnit(niceUnit(ci, rateUnits));
        const char * const coUnit(niceUnit(co, rateUnits));
/*      const char * const aiUnit(niceUnit(ai, rateUnits));
        const char * const aoUnit(niceUnit(ao, rateUnits)); */
        const char * const caoUnit(niceUnit(cao, rateUnits));
        const char * const caiUnit(niceUnit(cai, rateUnits));

        const char* phoneNumber = fDevice->getPhoneNumber();
        snprintf(status, sizeof status,
           /*   _("Interface %s:\n"
                  "  Current rate (in/out):\t%li %s/%li %s\n"
                  "  Current average (in/out):\t%lli %s/%lli %s\n"
                  "  Total average (in/out):\t%li %s/%li %s\n"
                  "  Transferred (in/out):\t%lli %s/%lli %s\n"
                  "  Online time:\t%ld:%02ld:%02ld"
                  "%s%s"), */
                _("Interface %s:\n"
                  "  Current rate (in/out):\t%li %s/%li %s\n"
                  "  Current average (in/out):\t%lli %s/%lli %s\n"
                  "  Transferred (in/out):\t%lli %s/%lli %s\n"
                  "  Online time:\t%ld:%02ld:%02ld"
                  "%s%s"),
                fDevName.c_str(),
                ci, ciUnit, co, coUnit,
                cai, caiUnit, cao, caoUnit,
/*              ai, aiUnit, ao, aoUnit, */
                vi, viUnit, vo, voUnit,
                period / 3600, period / 60 % 60, period % 60,
                *phoneNumber ? _("\n  Caller id:\t") : "", phoneNumber);
    } else {
        snprintf(status, sizeof status, "%.50s:", fDevName.c_str());
    }

    setToolTip(status);
}

void NetStatus::handleClick(const XButtonEvent &up, int count) {
    if (up.button == 1) {
        if (taskBarLaunchOnSingleClick ? count == 1 : !(count % 2)) {
            if (up.state & ControlMask) {
                start_time = monotime();
                start_ibytes = cur_ibytes;
                start_obytes = cur_obytes;
            } else {
                if (netCommand && netCommand[0])
                    smActionListener->runCommandOnce(netClassHint, netCommand);
            }
        }
    }
}

void NetStatus::paint(Graphics &g, const YRect &/*r*/) {
    long h = height();

    long b_in_max = 0;
    long b_out_max = 0;

    for (int i = 0; i < taskBarNetSamples; i++) {
        long in = ppp_in[i];
        long out = ppp_out[i];
        if (in > b_in_max)
            b_in_max = in;
        if (out > b_out_max)
            b_out_max = out;
    }

    long maxBytes = max(b_in_max + b_out_max, 1024L);

    ///!!! this should really be unified with acpustatus.cc
    for (int i = 0; i < taskBarNetSamples; i++) {
        if (1 /* ppp_in[i] > 0 || ppp_out[i] > 0 */) {
            long round = maxBytes / h / 2;
            int inbar, outbar;

            if ((inbar = (h * (long long) (ppp_in[i] + round)) / maxBytes)) {
                g.setColor(color[0]);   /* h - 1 means bottom */
                g.drawLine(i, h - 1, i, h - inbar);
            }

            if ((outbar = (h * (long long) (ppp_out[i] + round)) / maxBytes)) {
                g.setColor(color[1]);   /* 0 means top */
                g.drawLine(i, 0, i, outbar - 1);
            }

            if (inbar + outbar < h) {
                int l = outbar, t = h - inbar - 1;
                /*
                 g.setColor(color[2]);
                 //g.drawLine(i, 0, i, h - tot - 2);
                 g.drawLine(i, l, i, t - l);
                 */
                if (color[2]) {
                    g.setColor(color[2]);
                    g.drawLine(i, l, i, t);
                } else {
                    ref<YImage> gradient(parent()->getGradient());

                    if (gradient != null)
                        g.drawImage(gradient,
                                     x() + i, y() + l, width(), t - l, i, l);
                    else
                        if (taskbackPixmap != null)
                            g.fillPixmap(taskbackPixmap,
                                         i, l, width(), t - l, x() + i, y() + l);
                }
            }
        } else { /* Not reached: */
            if (color[2]) {
                g.setColor(color[2]);
                g.drawLine(i, 0, i, h - 1);
            } else {
                ref<YImage> gradient(parent()->getGradient());

                if (gradient != null)
                    g.drawImage(gradient,
                                 x() + i, y(), width(), h, i, 0);
                else
                    if (taskbackPixmap != null)
                        g.fillPixmap(taskbackPixmap,
                                     i, 0, width(), h, x() + i, y());
            }
        }
    }
}

/**
 * Check isdnstatus, by parsing /dev/isdninfo.
 *
 * Need read-access on /dev/isdninfo.
 */
#ifdef __linux__
bool NetIsdnDevice::isUp() {
    csmart str(load_text_file("/dev/isdninfo"));
    char val[5][32];
    int busage = 0;
    int bflags = 0;

    for (char* p = str; p && *p && p[1]; p = strchr(p, '\n')) {
        p += strspn(p, " \n");
        if (strncmp(p, "flags:", 6) == 0) {
            sscanf(p, "%s %s %s %s %s", val[0], val[1], val[2], val[3], val[4]);
            for (int i = 0; i < 4; i++) {
                if (strcmp(val[i+1], "0") != 0)
                    bflags |= (1 << i);
            }
        }
        else if (strncmp(p, "usage:", 6) == 0) {
            sscanf(p, "%s %s %s %s %s", val[0], val[1], val[2], val[3], val[4]);
            for (int i = 0; i < 4; i++) {
                if (strcmp(val[i+1], "0") != 0)
                    busage |= (1 << i);
            }
        }
        else if (strncmp(p, "phone:", 6) == 0) {
            sscanf(p, "%s %s %s %s %s", val[0], val[1], val[2], val[3], val[4]);
            for (int i = 0; i < 4; i++) {
                if (strncmp(val[i+1], "?", 1) != 0)
                    strlcpy(phoneNumber, val[i + 1], sizeof phoneNumber);
            }
        }
    }

    //msg("dbs: flags %d usage %d", bflags, busage);

    return bflags && busage;
}
#endif // ifdef __linux__

#if defined (__NetBSD__) || defined (__OpenBSD__)
bool NetOpenDevice::isUp() {
    bool up = false;
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != -1) {
        struct ifreq ifr;
        strlcpy(ifr.ifr_name, fDevName, IFNAMSIZ);
        if (ioctl(s, SIOCGIFFLAGS, (caddr_t)&ifr) != -1) {
            up = (ifr.ifr_flags & IFF_UP);
        }
        close(s);
    }
    return up;
}
#endif

#if defined (__FreeBSD__)
bool NetFreeDevice::isUp() {
    struct ifmibdata ifmd;
    size_t ifmd_size = sizeof ifmd;
    int nr_network_devs;
    size_t int_size = sizeof nr_network_devs;
    int name[6];
    name[0] = CTL_NET;
    name[1] = PF_LINK;
    name[2] = NETLINK_GENERIC;
    name[3] = IFMIB_IFDATA;
    name[5] = IFDATA_GENERAL;

    if (sysctlbyname("net.link.generic.system.ifcount", &nr_network_devs,
                    &int_size, (void*)0, 0) == -1) {
        printf("%s@%d: %s\n", __FILE__, __LINE__, strerror(errno));
    } else {
        for (int i = 1; i <= nr_network_devs; i++) {
            name[4] = i; /* row of the ifmib table */

            if (sysctl(name, 6, &ifmd, &ifmd_size, (void *)0, 0) == -1) {
                printf("%s@%d: %s\n", __FILE__, __LINE__, strerror(errno));
                continue;
            }
            if (fDevName == ifmd.ifmd_name) {
                return (ifmd.ifmd_flags & IFF_UP);
            }
        }
    }
    return false;
}
#endif

#ifdef __linux__
bool NetLinuxDevice::isUp() {
    int s = socket(PF_INET, SOCK_STREAM, 0);
    if (s == -1)
        return false;

    struct ifreq ifr;
    strlcpy(ifr.ifr_name, fDevName, IFNAMSIZ);
    bool up = (ioctl(s, SIOCGIFFLAGS, &ifr) >= 0 && (ifr.ifr_flags & IFF_UP));
    close(s);
    return up;
}
#endif

void NetStatus::updateStatus(const void* sharedData) {
    int last = taskBarNetSamples - 1;

    for (int i = 0; i < last; i++) {
        ppp_in[i] = ppp_in[i + 1];
        ppp_out[i] = ppp_out[i + 1];
    }
    getCurrent(&ppp_in[last], &ppp_out[last], sharedData);
    /* These two lines clears first measurement; you can throw these lines
     * off, but bug will occur: on startup, the _second_ bar will show
     * always zero -stibor- */
    if (!wasUp)
        ppp_in[last] = ppp_out[last] = 0;

    repaint();
}


#ifdef __linux__
void NetLinuxDevice::getCurrent(netbytes *in, netbytes *out, const void* sharedData) {
#if defined(__FreeBSD_kernel__)
    NetFreeDevice(fDevName).getCurrent(in, out, sharedData);
#else
    const char *p = (const char*) sharedData;
    if (p)
        sscanf(p, "%llu %*d %*d %*d %*d %*d %*d %*d %llu", in, out);

#endif
}
#endif //__linux__

#if defined(__FreeBSD__) || defined(__FreeBSD_kernel__)
void NetFreeDevice::getCurrent(netbytes *in, netbytes *out, const void* sharedData) {
    // FreeBSD code by Ronald Klop <ronald@cs.vu.nl>
    struct ifmibdata ifmd;
    size_t ifmd_size = sizeof ifmd;
    int nr_network_devs;
    size_t int_size = sizeof nr_network_devs;
    int name[6];
    name[0] = CTL_NET;
    name[1] = PF_LINK;
    name[2] = NETLINK_GENERIC;
    name[3] = IFMIB_IFDATA;
    name[5] = IFDATA_GENERAL;

    if (sysctlbyname("net.link.generic.system.ifcount", &nr_network_devs,
                    &int_size, (void*)0, 0) == -1) {
        printf("%s@%d: %s\n", __FILE__, __LINE__, strerror(errno));
    } else {
        for (int i = 1; i <= nr_network_devs; i++) {
            name[4] = i; /* row of the ifmib table */

            if (sysctl(name, 6, &ifmd, &ifmd_size, (void *)0, 0) == -1) {
                warn("%s@%d: %s\n", __FILE__, __LINE__, strerror(errno));
                continue;
            }
            if (ifmd.ifmd_name == fDevName) {
                *in = ifmd.ifmd_data.ifi_ibytes;
                *out = ifmd.ifmd_data.ifi_obytes;
                break;
            }
        }
    }
}
#endif //FreeBSD

#if defined(__OpenBSD__) || defined(__NetBSD__)
void NetOpenDevice::getCurrent(netbytes *in, netbytes *out, const void* sharedData) {
    int s;

    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s != -1) {
#ifdef __OpenBSD__
        struct ifreq ifdr;
        struct if_data ifi;
        strlcpy(ifdr.ifr_name, fDevName, IFNAMSIZ);
        ifdr.ifr_data = (caddr_t) &ifi;
#endif
#ifdef __NetBSD__
        struct ifdatareq ifdr;
        struct if_data& ifi = &ifdr.ifdr_data;
        strlcpy(ifdr.ifdr_name, fDevName, IFNAMSIZ);
#endif
        if (ioctl(s, SIOCGIFDATA, &ifdr) != -1) {
            *in = ifi.ifi_ibytes;
            *out = ifi.ifi_obytes;
        }
        close(s);
    }
}
#endif //__OpenBSD__

void NetStatus::getCurrent(long *in, long *out, const void* sharedData) {
    cur_ibytes = 0;
    cur_obytes = 0;

    fDevice->getCurrent(&cur_ibytes, &cur_obytes, sharedData);

    // correct the values and look for overflows
    //msg("w/o corrections: ibytes: %lld, prev_ibytes; %lld, offset: %lld", cur_ibytes, prev_ibytes, offset_ibytes);

    cur_ibytes += offset_ibytes;
    cur_obytes += offset_obytes;

    if (cur_ibytes < prev_ibytes)
        // har, har, overflow. Use the recent prev_ibytes value as offset this time
        cur_ibytes = offset_ibytes = prev_ibytes;

    if (cur_obytes < prev_obytes)
        // har, har, overflow. Use the recent prev_obytes value as offset this time
        cur_obytes = offset_obytes = prev_obytes;

    timeval curr_time = monotime();
    double delta_t = max(0.001, toDouble(curr_time - prev_time));

    if (in && out) {
        *in  = (long) ((cur_ibytes - prev_ibytes) / delta_t);
        *out = (long) ((cur_obytes - prev_obytes) / delta_t);
    }

    prev_time = curr_time;
    prev_ibytes = cur_ibytes;
    prev_obytes = cur_obytes;
}

NetStatusControl::~NetStatusControl() {
}

#ifdef __linux__
void NetStatusControl::fetchSystemData() {
    devStats.clear();
    devicesText = load_text_file("/proc/net/dev");
    if (devicesText == 0)
        return;

    for (char* p = devicesText; (p = strchr(p, '\n')) != 0; ) {
        *p = 0;
        while (*++p == ' ');
        char* name = p;
        p += strcspn(p, " :|\n");
        if (*p == ':') {
            *p = 0;
            while (*++p && *p == ' ');
            if (*p && *p != '\n')
                devStats.append(netpair(name, p));
        }
    }
}
#endif

NetStatusControl::NetStatusControl(IApp* app, YSMListener* smActionListener,
        IAppletContainer* taskBar, YWindow* aParent) :
    app(app),
    smActionListener(smActionListener),
    taskBar(taskBar),
    aParent(aParent)
{
    MStringArray interfaces;
    mstring devName, devList(netDevice);
    while (devList.splitall(' ', &devName, &devList)) {
        if (devName.isEmpty())
            continue;
        cstring devStr(devName);

        if (strpbrk(devStr, "*?[]\\.")) {
            if (interfaces.getCount() == 0)
                getInterfaces(interfaces);
            MStringArray::IterType iter = interfaces.iterator();
            while (++iter) {
                cstring cstr(*iter);
                if (fnmatch(devStr, cstr, 0) == 0) {
                    IterType have = getIterator();
                    while (++have && have->name() != cstr);
                    if (have == false)
                        createNetStatus(cstr);
                }
            }
            patterns.append(devName);
        }
        else {
            unsigned index = if_nametoindex(devStr);
            if (1 <= index)
                createNetStatus(devStr);
            else
                patterns.append(devName);
        }
    }

    fUpdateTimer->setTimer(taskBarNetDelay, this, true);
}

NetStatus* NetStatusControl::createNetStatus(cstring netdev) {
    NetStatus* status = new NetStatus(app, smActionListener,
                                      netdev, taskBar, aParent);
    fNetStatus.append(status);
    return status;
}

void NetStatusControl::getInterfaces(MStringArray& names)
{
    names.clear();
    char name[IF_NAMESIZE];
    unsigned const stop(99);
    for (unsigned index = 1; index < stop; ++index) {
        if (if_indextoname(index, name)) {
            mstring mstr(name);
            names.append(mstr);
        } else {
            break;
        }
    }
}

bool NetStatusControl::handleTimer(YTimer *t)
{
    if (t != fUpdateTimer)
        return false;

#ifdef __linux__
    linuxUpdate();
#else
    for (IterType iter = getIterator(); ++iter; )
        iter->timedUpdate(0);
#endif

    return true;
}

#ifdef __linux__
void NetStatusControl::linuxUpdate() {
    fetchSystemData();

    int const count(fNetStatus.getCount());
    bool covered[count] = {};

    for (IterStats stat = devStats.iterator(); ++stat; ) {
        const char* name = (*stat).left;
        const char* data = (*stat).right;
        IterType iter = getIterator();
        while (++iter && iter->name() != name);
        if (iter && iter.where() < count) {
            iter->timedUpdate(data);
            covered[iter.where()] = true;
            continue;
        }

        // oh, we got a new device? allowed?
        // XXX: this still wastes some cpu cycles
        // for repeated fnmatch on forbidden devices.
        // Maybe tackle this with a list of checksums?
        MStringArray::IterType pat = patterns.iterator();
        while (++pat && fnmatch(cstring(*pat), name, 0));
        if (pat) {
            createNetStatus(name)->timedUpdate(data);
        }
    }
    // mark disappeared devices as down without additional ioctls
    for (int i = 0; i < count; ++i)
        if (covered[i] == false)
            fNetStatus[i]->timedUpdate(0, true);

    devStats.clear();
    devicesText = 0;
}
#endif

#endif // HAVE_NET_STATUS

// vim: set sw=4 ts=4 et:
