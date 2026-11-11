#include <iostream>
#include <cassert>
#include "haptic_link.h"

int main() {
    // test command mapping
    assert(commandToByte(Command::LEFT) == '1');
    assert(commandToByte(Command::RIGHT) == '2');
    assert(commandToByte(Command::STOP) == '3');
    assert(commandToByte(Command::GO) == '4');

    // test sending commands without hardware
    HapticLink link;
    link.setCooldownMs(0); // no cooldown for testing

    assert(link.send(Command::LEFT));
    assert(link.send(Command::RIGHT));
    assert(link.send(Command::STOP));
    assert(link.send(Command::GO));
    assert(!link.send(Command::INVALID)); // shouldn't send invalid command

    // test cooldown
    HapticLink cooldownLink;
    cooldownLink.setCooldownMs(60000); // 60 second cooldown

    assert(cooldownLink.send(Command::LEFT));
    assert(!cooldownLink.send(Command::RIGHT)); // should fail because cooldown
    assert(cooldownLink.send(Command::STOP)); 

    std::cout << "All haptics tests passed!\n";
    return 0;
}