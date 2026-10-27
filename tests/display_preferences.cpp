#include "../saltysd_proc/source/display_preferences.hpp"
#include <cassert>
#include <iostream>

using namespace display_preferences;

int main() {
    const auto defaults = parse("");
    assert(defaults.allowed[2] && defaults.allowed[4] && !defaults.allowed[13]);
    assert(defaults.preferred == 0 && defaults.preferred720 == 0);
    const auto settings = parse(
        "; retained comments\r\n[Other]\npreferredRefreshRate=75\n[Common]\r\n"
        "tvName=Monitor with spaces\nrefreshRateAllowed={50, 60, 120}\n"
        "refreshRateAllowed720p={50,60,90}\npreferredRefreshRate=120\n"
        "preferredRefreshRate720p=90\nallowPatchesToForce60InDocked=False\n"
        "matchLowestRefreshRate=True\nbringDefaultRefreshRateWhenOutOfFocus=True\n"
        "displayDisclaimerAcknowledged=True\n");
    assert(settings.preferred == 120 && settings.preferred720 == 90);
    assert(settings.dontForce60 && settings.matchLowest && settings.outOfFocus60);
    const Link monitorA{0x12345678, 1920, 1080, 20, 2};
    auto monitorB = monitorA;
    monitorB.edid = 0x87654321;
    auto hd = monitorA;
    hd.width = 1280;
    hd.height = 720;
    assert(preferredTarget(settings, monitorA, 120) == 120);
    assert(preferredTarget(settings, monitorA, 75) == 0); // changed dock/link
    assert(preferredTarget(settings, hd, 120) == 90);    // independent resolution
    assert(preferredTarget(defaults, monitorB, 120) == 0); // no monitor inheritance
    hd.height = 480;
    assert(preferredTarget(settings, hd, 120) == 0);
    auto disallowed = settings;
    disallowed.allowed[13] = false;
    assert(preferredTarget(disallowed, monitorA, 120) == 0);
    for (const auto bad : {"", "0", "-120", "120Hz", "1200", "256", "121", "90.0", "120junk", "+120"}) {
        assert(parseRate(bad) == 0);
    }
    assert(parseRate(" 120 \r") == 120);
    for (const auto malformed : {"", "{", "{120,}", "{120,no}", "{120", "120}"}) {
        assert(parseAllowed(malformed) == defaults.allowed);
    }
    assert(parse("[Other]\npreferredRefreshRate=120").preferred == 0);
    assert(parse("[Common]\npreferredRefreshRate=120\npreferredRefreshRate=bad").preferred == 0);
    assert(!parse("[Common]\nmatchLowestRefreshRate=Truejunk").matchLowest);

    Connection connection;
    using O = Connection::Observation;
    assert(connection.observe(monitorA, 0) == O::Waiting);
    assert(!connection.shouldApply(false, 0));
    assert(connection.observe(monitorA, 500) == O::Waiting);
    assert(connection.observe(monitorA, 1000) == O::Activated);
    assert(!connection.shouldApply(true, 1000)); // Display Sync wins, no consumed attempt
    assert(connection.shouldApply(false, 1000));
    connection.attempted(true, 1000);
    for (uint64_t now = 1250; now <= 10000; now += 250) {
        assert(connection.observe(monitorA, now) == O::Ready);
        assert(!connection.shouldApply(false, now)); // no continual mode forcing
    }
    assert(connection.observe(monitorB, 10250) == O::Waiting);
    assert(!connection.ready());
    assert(connection.observe(monitorB, 11250) == O::Activated);
    assert(connection.shouldApply(false, 11250));
    connection.attempted(false, 11250);
    assert(!connection.shouldApply(false, 11500));
    assert(connection.shouldApply(false, 12250));
    connection.attempted(false, 12250);
    assert(connection.shouldApply(false, 13250));
    connection.attempted(false, 13250);
    assert(!connection.shouldApply(false, 15000)); // bounded failures

    connection.disconnect(); // HDMI disconnect while still docked
    assert(!connection.ready());
    assert(connection.observe(monitorA, 16000) == O::Waiting);
    assert(connection.observe(monitorA, 17000) == O::Activated);
    connection.manualSelection();
    assert(!connection.shouldApply(false, 17000));
    assert(connection.observe(monitorA, 22000) == O::Waiting); // suspend/resume gap
    assert(connection.observe(monitorA, 23000) == O::Activated);
    assert(connection.shouldApply(false, 23000));
    auto changedLink = monitorA;
    changedLink.rate = 10;
    assert(connection.observe(changedLink, 23250) == O::Waiting);
    assert(connection.observe(monitorA, 23500) == O::Waiting); // restart settling
    assert(connection.observe(monitorA, 24250) == O::Waiting);
    assert(connection.observe(monitorA, 24500) == O::Activated);
    hd = monitorA;
    hd.width = 1280;
    hd.height = 720;
    assert(connection.observe(hd, 24750) == O::Waiting);
    assert(connection.observe(hd, 25750) == O::Activated);
    assert(connection.observe(hd, 10) == O::Waiting); // reset/wrap cannot underflow
    std::cout << "Display preference parser and connection scenarios passed\n";
}
