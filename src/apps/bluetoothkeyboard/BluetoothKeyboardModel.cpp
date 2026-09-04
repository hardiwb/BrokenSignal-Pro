#include "apps/bluetoothkeyboard/BluetoothKeyboardInternal.h"

#include <cmath>
#include <cstring>

#include "core/Keyboard.h"
#include "core/State.h"
#include "core/System.h"
#include "module/service/Bluetooth.h"
#include "UI/Footer.h"
#include "UI/Header.h"
#include "UI/List.h"
#include "UI/Overlay.h"

namespace BluetoothKeyboardInternal
{
namespace
{
uint8_t lastReport[8] = {};
bool lastReportValid = false;
bool mouseModeActive = false;
bool mouseToggleChordDown = false;
bool suppressMouseExitKey = false;
bool invertMouseY = true;
uint8_t mouseSensitivity = 100;
uint8_t configuredMouseScreens = 1;
uint8_t activeMouseScreen = 0;
uint8_t lastMouseButtons = 0;
bool lastScrollUp = false;
bool lastScrollDown = false;
bool lastScrollLeft = false;
bool lastScrollRight = false;
bool lastCenterKey = false;
bool lastScreenKey = false;
uint8_t lastSegmentKey = 0;
float smoothMouseX = 0.0f;
float smoothMouseY = 0.0f;
float mouseRemainderX = 0.0f;
float mouseRemainderY = 0.0f;
int32_t mousePositionX = 16384;
int32_t mousePositionY = 16384;
unsigned long lastMouseUpdateMs = 0;
unsigned long mouseFreezeUntilMs = 0;
bool renaming = false;
String renameValue;

constexpr unsigned long MOUSE_UPDATE_INTERVAL_MS = 10;
constexpr unsigned long CLICK_FREEZE_MS = 120;
constexpr float GYRO_DEADZONE = 1.5f;
constexpr float MIN_MOUSE_SENSITIVITY = 0.05f;
constexpr float MAX_MOUSE_SENSITIVITY = 0.25f;
constexpr float FAST_GYRO_SPEED = 120.0f;
constexpr float MOUSE_SMOOTHING_ALPHA = 0.25f;
constexpr int32_t MOUSE_POSITION_CENTER = 16384;
constexpr int32_t MOUSE_POSITION_MAX = 32767;
constexpr int32_t MOUSE_POSITION_RANGE = 32768;
constexpr int32_t ABSOLUTE_UNITS_PER_PIXEL = 16;
constexpr uint8_t MOUSE_SENSITIVITY_LEVELS[] = {50, 75, 100, 150, 200};

bool hidKeyDown(const Keyboard_Class::KeysState &keys, uint8_t usage)
{
    for (uint8_t key : keys.hid_keys)
    {
        if (key == usage)
            return true;
    }
    return false;
}

int32_t activeScreenWidth()
{
    return MOUSE_POSITION_RANGE / configuredMouseScreens;
}

int32_t activeScreenLeft()
{
    return activeMouseScreen * activeScreenWidth();
}

int32_t activeScreenCenterX()
{
    return activeScreenLeft() + activeScreenWidth() / 2;
}

void moveToSegment(uint8_t key)
{
    uint8_t column = 0;
    bool bottom = false;
    switch (key)
    {
    case 0x2D: column = 0; break;                // -: top-left
    case 0x2E: column = 1; break;                // =: top-middle
    case 0x2A: column = 2; break;                // Backspace: top-right
    case 0x2F: column = 0; bottom = true; break; // [: bottom-left
    case 0x30: column = 1; bottom = true; break; // ]: bottom-middle
    case 0x31: column = 2; bottom = true; break; // \: bottom-right
    default: return;
    }
    mousePositionX = activeScreenLeft() +
        activeScreenWidth() * (column * 2 + 1) / 6;
    mousePositionY = MOUSE_POSITION_RANGE * (bottom ? 3 : 1) / 4;
}

int16_t mouseAxisDelta(float &remainder, float movement)
{
    remainder += movement;
    const int wholePixels = constrain(
        static_cast<int>(remainder), -127, 127);
    remainder -= wholePixels;
    return static_cast<int16_t>(wholePixels);
}

void resetMouseTracking()
{
    lastMouseButtons = 0;
    lastScrollUp = false;
    lastScrollDown = false;
    lastScrollLeft = false;
    lastScrollRight = false;
    lastCenterKey = false;
    lastScreenKey = false;
    lastSegmentKey = 0;
    smoothMouseX = 0.0f;
    smoothMouseY = 0.0f;
    mouseRemainderX = 0.0f;
    mouseRemainderY = 0.0f;
    lastMouseUpdateMs = 0;
    mouseFreezeUntilMs = 0;
}

void tickGyroMouse(const Keyboard_Class::KeysState &keys)
{
    const unsigned long now = millis();
    if (lastMouseUpdateMs != 0 &&
        now - lastMouseUpdateMs < MOUSE_UPDATE_INTERVAL_MS)
        return;
    lastMouseUpdateMs = now;

    const uint8_t buttons =
        (keys.enter ? 0x01 : 0x00) |
        (keys.opt ? 0x02 : 0x00) |
        (keys.alt ? 0x04 : 0x00);
    const bool scrollUp = hidKeyDown(keys, 0x33);    // ;
    const bool scrollDown = hidKeyDown(keys, 0x37);  // .
    const bool scrollLeft = hidKeyDown(keys, 0x36);  // ,
    const bool scrollRight = hidKeyDown(keys, 0x38); // /
    const int8_t wheel = static_cast<int8_t>(
        (scrollUp && !lastScrollUp ? 1 : 0) -
        (scrollDown && !lastScrollDown ? 1 : 0));
    const int8_t horizontalWheel = static_cast<int8_t>(
        (scrollRight && !lastScrollRight ? 1 : 0) -
        (scrollLeft && !lastScrollLeft ? 1 : 0));
    const bool centerKey = keys.ctrl;
    const bool recenter = centerKey && !lastCenterKey;
    const bool screenKey = keys.tab;
    const bool switchScreen = configuredMouseScreens > 1 &&
        screenKey && !lastScreenKey;
    uint8_t segmentKey = 0;
    if (hidKeyDown(keys, 0x2D)) segmentKey = 0x2D;      // -: top-left
    else if (hidKeyDown(keys, 0x2E)) segmentKey = 0x2E; // =: top-middle
    else if (keys.del) segmentKey = 0x2A;               // Backspace: top-right
    else if (hidKeyDown(keys, 0x2F)) segmentKey = 0x2F; // [: bottom-left
    else if (hidKeyDown(keys, 0x30)) segmentKey = 0x30; // ]: bottom-middle
    else if (hidKeyDown(keys, 0x31)) segmentKey = 0x31; // \: bottom-right
    const bool moveSegment = segmentKey != 0 &&
        segmentKey != lastSegmentKey;

    if (buttons != lastMouseButtons)
        mouseFreezeUntilMs = now + CLICK_FREEZE_MS;

    int16_t moveX = 0;
    int16_t moveY = 0;
    float gyroX = 0.0f;
    float gyroY = 0.0f;
    float gyroZ = 0.0f;
    if (keys.space)
    {
        // Space acts as a gyro clutch: discard filtered and fractional
        // movement while the user readjusts their hand position.
        smoothMouseX = 0.0f;
        smoothMouseY = 0.0f;
        mouseRemainderX = 0.0f;
        mouseRemainderY = 0.0f;
    }
    else if (M5.Imu.isEnabled() && M5.Imu.getGyro(&gyroX, &gyroY, &gyroZ))
    {
        // With the Cardputer held in landscape, pitch controls vertical
        // movement and yaw controls horizontal movement.
        float activeX = -gyroZ;
        float activeY = invertMouseY ? -gyroX : gyroX;
        if (fabsf(activeX) < GYRO_DEADZONE)
            activeX = 0.0f;
        if (fabsf(activeY) < GYRO_DEADZONE)
            activeY = 0.0f;

        const float speed = sqrtf(activeX * activeX + activeY * activeY);
        const float speedRatio = min(1.0f, speed / FAST_GYRO_SPEED);
        const float sensitivity = MIN_MOUSE_SENSITIVITY +
            (MAX_MOUSE_SENSITIVITY - MIN_MOUSE_SENSITIVITY) * speedRatio;
        const float sensitivityScale = mouseSensitivity / 100.0f;
        const float targetX = activeX * sensitivity * sensitivityScale;
        const float targetY = activeY * sensitivity * sensitivityScale;
        smoothMouseX += MOUSE_SMOOTHING_ALPHA * (targetX - smoothMouseX);
        smoothMouseY += MOUSE_SMOOTHING_ALPHA * (targetY - smoothMouseY);

        if (now >= mouseFreezeUntilMs)
        {
            moveX = mouseAxisDelta(mouseRemainderX, smoothMouseX);
            moveY = mouseAxisDelta(mouseRemainderY, smoothMouseY);
        }
    }

    if (switchScreen)
    {
        activeMouseScreen = (activeMouseScreen + 1) % configuredMouseScreens;
        mousePositionX = activeScreenCenterX();
        mousePositionY = MOUSE_POSITION_CENTER;
    }
    if (moveSegment)
        moveToSegment(segmentKey);
    else if (recenter)
    {
        mousePositionX = activeScreenCenterX();
        mousePositionY = MOUSE_POSITION_CENTER;
    }
    else if (!switchScreen)
    {
        mousePositionX = constrain(
            mousePositionX + moveX * ABSOLUTE_UNITS_PER_PIXEL,
            static_cast<long>(activeScreenLeft()),
            static_cast<long>(activeScreenLeft() + activeScreenWidth() - 1));
        mousePositionY = constrain(
            mousePositionY + moveY * ABSOLUTE_UNITS_PER_PIXEL,
            0L, static_cast<long>(MOUSE_POSITION_MAX));
    }

    if (recenter || switchScreen || moveSegment)
    {
        smoothMouseX = 0.0f;
        smoothMouseY = 0.0f;
        mouseRemainderX = 0.0f;
        mouseRemainderY = 0.0f;
    }

    if (moveX != 0 || moveY != 0 || wheel != 0 || horizontalWheel != 0 ||
        buttons != lastMouseButtons || recenter || switchScreen || moveSegment)
    {
        BluetoothService::sendMouseReport(
            buttons,
            static_cast<uint16_t>(mousePositionX),
            static_cast<uint16_t>(mousePositionY), wheel, horizontalWheel);
        lastActivityMs = now;
        if (!screenOn)
            wakeScreen();
    }

    lastMouseButtons = buttons;
    lastScrollUp = scrollUp;
    lastScrollDown = scrollDown;
    lastScrollLeft = scrollLeft;
    lastScrollRight = scrollRight;
    lastCenterKey = centerKey;
    lastScreenKey = screenKey;
    lastSegmentKey = segmentKey;
}

uint8_t fnMappedKey(uint8_t key)
{
    switch (key)
    {
    case 0x35: return 0x29; // ` -> Escape
    case 0x33: return 0x52; // ; -> Up
    case 0x37: return 0x51; // . -> Down
    case 0x36: return 0x50; // , -> Left
    case 0x38: return 0x4F; // / -> Right
    case 0x2A: return 0x4C; // Backspace -> Forward Delete
    default: return key;
    }
}

void buildKeyboardReport(const Keyboard_Class::KeysState &keys, uint8_t report[8])
{
    memset(report, 0, 8);
    report[0] = keys.modifiers;
    if (keys.opt)
        report[0] |= 0x08; // Cardputer Opt becomes Left GUI / Command.

    int outputIndex = 2;
    for (uint8_t key : keys.hid_keys)
    {
        if (outputIndex >= 8)
            break;
        report[outputIndex++] = keys.fn ? fnMappedKey(key) : key;
    }
}

int itemCount()
{
    return BluetoothService::bondCount() +
           (BluetoothService::canPairNew() ? 1 : 0);
}

ListModel buildListModel()
{
    ListModel model;
    model.selected = selected;
    model.scrollTop = scrollTop;

    for (int i = 0; i < BluetoothService::bondCount(); ++i)
    {
        ListItemModel item;
        item.type = ListItemType::Property;
        const String savedName = BluetoothService::bondName(i);
        item.label = savedName.length() > 0
            ? savedName : "PC " + String(i + 1);
        item.value = BluetoothService::bondAddressText(i);
        item.isSelected = i == selected;
        model.items.push_back(item);
    }

    if (BluetoothService::canPairNew())
    {
        ListItemModel item;
        item.type = ListItemType::Normal;
        item.label = "Pair New PC";
        item.isSelected = selected == BluetoothService::bondCount();
        model.items.push_back(item);
    }
    return model;
}
} // namespace

int selected = 0;
int scrollTop = 0;

void initialize()
{
    BluetoothService::begin();
    invertMouseY = BluetoothService::mouseInvertY();
    mouseSensitivity = BluetoothService::mouseSensitivityPercent();
    bool validSensitivity = false;
    for (uint8_t level : MOUSE_SENSITIVITY_LEVELS)
        validSensitivity = validSensitivity || mouseSensitivity == level;
    if (!validSensitivity)
        mouseSensitivity = 100;
    configuredMouseScreens = BluetoothService::mouseScreenCount();
    if (configuredMouseScreens < 1 || configuredMouseScreens > 2)
        configuredMouseScreens = 1;
    activeMouseScreen = 0;
    mousePositionX = activeScreenCenterX();
    mousePositionY = MOUSE_POSITION_CENTER;
}

void refreshBonds()
{
    BluetoothService::refreshBonds();
    selected = itemCount() > 0 ? constrain(selected, 0, itemCount() - 1) : 0;
    scrollTop = min(scrollTop, selected);
}

int bondCount()
{
    return BluetoothService::bondCount();
}

bool selectedIsBond()
{
    return selected >= 0 && selected < BluetoothService::bondCount();
}

String bondAddressText(int index)
{
    return BluetoothService::bondAddressText(index);
}

bool renameModalActive()
{
    return renaming;
}

bool mouseYInverted()
{
    return invertMouseY;
}

void toggleMouseYInverted()
{
    invertMouseY = !invertMouseY;
    BluetoothService::setMouseInvertY(invertMouseY);
}

uint8_t mouseSensitivityPercent()
{
    return mouseSensitivity;
}

void adjustMouseSensitivity(int direction)
{
    constexpr int count =
        sizeof(MOUSE_SENSITIVITY_LEVELS) /
        sizeof(MOUSE_SENSITIVITY_LEVELS[0]);
    int index = 2;
    for (int i = 0; i < count; ++i)
    {
        if (MOUSE_SENSITIVITY_LEVELS[i] == mouseSensitivity)
        {
            index = i;
            break;
        }
    }
    mouseSensitivity = MOUSE_SENSITIVITY_LEVELS[
        (index + direction + count) % count];
    BluetoothService::setMouseSensitivityPercent(mouseSensitivity);
}

uint8_t mouseScreenCount()
{
    return configuredMouseScreens;
}

void adjustMouseScreenCount(int)
{
    configuredMouseScreens = configuredMouseScreens == 1 ? 2 : 1;
    activeMouseScreen = 0;
    mousePositionX = activeScreenCenterX();
    mousePositionY = MOUSE_POSITION_CENTER;
    BluetoothService::setMouseScreenCount(configuredMouseScreens);
}

void drawListScreen()
{
    refreshBonds();

    HeaderModel header;
    header.appHeaderTag = "BT KEYBOARD";
    header.appHeaderTitle = "Paired Devices";
    header.cursor = true;
    drawHeader(header);
    drawList(buildListModel());

    FooterModel footer;
    footer.left = "[;/.]Move [Ok]Open";
    footer.center = "[Opt]Manage";
    footer.battery = footerBatteryText();
    drawFooter(footer);
}

void drawSessionOverlay()
{
    if (!BluetoothService::keyboardSessionActive())
        return;

    OverlayModel model;
    model.type = OverlayType::Message;
    model.title = "BLUETOOTH KEYBOARD";
    model.confirmText = "[BtnG0] Disconnect";

    switch (BluetoothService::keyboardLinkState())
    {
    case BluetoothService::KeyboardLinkState::Advertising:
        model.items.push_back(BluetoothService::targetBondSelected()
            ? "WAITING FOR DEVICE" : "PAIRING MODE");
        model.items.push_back(BluetoothService::targetBondSelected()
            ? BluetoothService::targetAddressText()
            : "Select on host Bluetooth");
        break;
    case BluetoothService::KeyboardLinkState::Connected:
        model.items.push_back("SECURING CONNECTION");
        model.items.push_back(BluetoothService::peerAddressText());
        break;
    case BluetoothService::KeyboardLinkState::Pairing:
    {
        char pin[16];
        snprintf(pin, sizeof(pin), "PIN %06lu",
                 (unsigned long)BluetoothService::pairingPasskey());
        model.items.push_back(pin);
        model.items.push_back("Enter PIN on host");
        break;
    }
    case BluetoothService::KeyboardLinkState::Ready:
        model.items.push_back("CONNECTED + ENCRYPTED");
        model.items.push_back(BluetoothService::peerAddressText());
        model.items.push_back("Fn+M starts gyro mouse");
        break;
    case BluetoothService::KeyboardLinkState::Failed:
        model.items.push_back("CONNECTION REJECTED");
        model.items.push_back(BluetoothService::keyboardFailureText());
        model.items.push_back("Forget on PC, then press BtnG0");
        break;
    case BluetoothService::KeyboardLinkState::Idle:
        model.items.push_back("DISCONNECTED");
        break;
    }
    drawOverlay(model);
}

void drawRenameOverlay()
{
    OverlayModel model;
    model.type = OverlayType::TextInput;
    model.title = "RENAME PAIRED PC";
    model.prompt = "Friendly name";
    model.value = renameValue;
    model.confirmText = "[Esc]Cancel   [Ok]Save";
    drawOverlay(model);
}

void moveSelection(int direction)
{
    const int count = itemCount();
    if (count <= 0)
        return;

    const int oldSelected = selected;
    const int oldScrollTop = scrollTop;
    selected = (selected + direction + count) % count;
    if (selected < scrollTop)
        scrollTop = selected;
    else if (selected >= scrollTop + LIST_VISIBLE_ITEM)
        scrollTop = selected - LIST_VISIBLE_ITEM + 1;

    if (oldScrollTop != scrollTop)
        drawListScreen();
    else
        drawListSelection(buildListModel(), oldSelected, selected);
}

void openSelected()
{
    const int bondIndex = selectedIsBond() ? selected : -1;
    memset(lastReport, 0, sizeof(lastReport));
    lastReportValid = false;
    mouseModeActive = false;
    mouseToggleChordDown = false;
    suppressMouseExitKey = false;
    resetMouseTracking();
    if (BluetoothService::startKeyboardSession(bondIndex))
        drawSessionOverlay();
}

void beginRenameSelected()
{
    if (!selectedIsBond())
        return;

    renameValue = BluetoothService::bondName(selected);
    if (renameValue.length() == 0)
        renameValue = "PC " + String(selected + 1);
    renaming = true;
    drawRenameOverlay();
}

void handleRenameInput(Keyboard_Class::KeysState &keys)
{
    if (keyboardBackPressed(keys))
    {
        renaming = false;
        drawListScreen();
        return;
    }

    if (keys.enter)
    {
        renameValue.trim();
        BluetoothService::setBondName(selected, renameValue);
        renaming = false;
        drawListScreen();
        return;
    }

    if (keys.del && renameValue.length() > 0)
    {
        renameValue.remove(renameValue.length() - 1);
        drawOverlayInputValue(renameValue);
        return;
    }

    for (char c : keys.word)
    {
        if (keyboardTextInputChar(keys, c) && renameValue.length() < 24)
        {
            renameValue += c;
            drawOverlayInputValue(renameValue);
        }
    }
}

void disconnectSession()
{
    BluetoothService::stopKeyboardSession();
    memset(lastReport, 0, sizeof(lastReport));
    lastReportValid = false;
    mouseModeActive = false;
    mouseToggleChordDown = false;
    suppressMouseExitKey = false;
    resetMouseTracking();
    drawListScreen();
}

void forgetSelectedBond()
{
    if (selectedIsBond())
        BluetoothService::forgetBond(selected);
    selected = 0;
    drawListScreen();
}

void forgetAllBonds()
{
    BluetoothService::forgetAllBonds();
    selected = 0;
    drawListScreen();
}

void consumeModalMatrixInput(Keyboard_Class::KeysState &)
{
    // Reports are polled in tick(), because M5's isChange() only compares the
    // key count and misses a same-count substitution.
}

void tick()
{
    if (!BluetoothService::keyboardSessionActive())
    {
        if (BluetoothService::takeUiDirty())
            drawListScreen();
        return;
    }

    if (M5Cardputer.BtnA.wasPressed())
    {
        disconnectSession();
        BluetoothService::takeUiDirty();
        return;
    }

    if (BluetoothService::keyboardReady())
    {
        const Keyboard_Class::KeysState &keys =
            M5Cardputer.Keyboard.keysState();
        const bool toggleChord = keys.fn && hidKeyDown(keys, 0x10); // Fn+M
        const bool togglePressed = toggleChord && !mouseToggleChordDown;
        mouseToggleChordDown = toggleChord;
        const bool exitKey = hidKeyDown(keys, 0x35); // `

        if (togglePressed && !mouseModeActive)
        {
            const uint8_t emptyKeyboard[8] = {};
            BluetoothService::sendKeyboardReport(emptyKeyboard);
            memcpy(lastReport, emptyKeyboard, sizeof(lastReport));
            lastReportValid = true;
            resetMouseTracking();
            mouseModeActive = true;
        }

        if (mouseModeActive && exitKey)
        {
            BluetoothService::sendMouseReport(
                0, static_cast<uint16_t>(mousePositionX),
                static_cast<uint16_t>(mousePositionY), 0);
            mouseModeActive = false;
            suppressMouseExitKey = true;
            resetMouseTracking();
            lastReportValid = false;
        }
        else if (suppressMouseExitKey && !exitKey)
            suppressMouseExitKey = false;

        if (mouseModeActive)
        {
            tickGyroMouse(keys);
        }
        else if (!toggleChord && !suppressMouseExitKey)
        {
            uint8_t report[8];
            buildKeyboardReport(keys, report);
            if (!lastReportValid ||
                memcmp(report, lastReport, sizeof(report)) != 0)
            {
                BluetoothService::sendKeyboardReport(report);
                memcpy(lastReport, report, sizeof(lastReport));
                lastReportValid = true;
                lastActivityMs = millis();
                if (!screenOn)
                    wakeScreen();
            }
        }
    }
    else
    {
        // A new BLE connection has a new notification channel. Resend the
        // complete matrix state when it becomes ready, even if no key changed.
        lastReportValid = false;
        mouseModeActive = false;
        mouseToggleChordDown = false;
        suppressMouseExitKey = false;
        resetMouseTracking();
    }

    if (BluetoothService::takeUiDirty())
    {
        refreshBonds();
        drawSessionOverlay();
    }
}
} // namespace BluetoothKeyboardInternal
