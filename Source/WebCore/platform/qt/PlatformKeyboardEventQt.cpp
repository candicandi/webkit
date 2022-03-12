/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformKeyboardEvent.h"

#include "NotImplemented.h"
#include "WindowsKeyboardCodes.h"

#include <QKeyEvent>
#include <ctype.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HexNumber.h>
#include <wtf/WallTime.h>

namespace WebCore {

String keyIdentifierForQtKeyCode(int keyCode)
{
    // Based on http://www.w3.org/TR/DOM-Level-3-Events/%23key-values-list
    switch (keyCode) {
    case Qt::Key_unknown:
        return "Unidentified"_s;
    case Qt::Key_Alt:
        return "Alt"_s;
    case Qt::Key_AltGr:
        return "AltGraph"_s;
    case Qt::Key_Control:
        return "Control"_s;
    case Qt::Key_Meta:
        return "Meta"_s;
    case Qt::Key_Shift:
        return "Shift"_s;
    case Qt::Key_Multi_key:
        return "Compose"_s;
    case Qt::Key_F1:
        return "F1"_s;
    case Qt::Key_F2:
        return "F2"_s;
    case Qt::Key_F3:
        return "F3"_s;
    case Qt::Key_F4:
        return "F4"_s;
    case Qt::Key_F5:
        return "F5"_s;
    case Qt::Key_F6:
        return "F6"_s;
    case Qt::Key_F7:
        return "F7"_s;
    case Qt::Key_F8:
        return "F8"_s;
    case Qt::Key_F9:
        return "F9"_s;
    case Qt::Key_F10:
        return "F10"_s;
    case Qt::Key_F11:
        return "F11"_s;
    case Qt::Key_F12:
        return "F12"_s;
    case Qt::Key_F13:
        return "F13"_s;
    case Qt::Key_F14:
        return "F14"_s;
    case Qt::Key_F15:
        return "F15"_s;
    case Qt::Key_F16:
        return "F16"_s;
    case Qt::Key_F17:
        return "F17"_s;
    case Qt::Key_F18:
        return "F18"_s;
    case Qt::Key_F19:
        return "F19"_s;
    case Qt::Key_F20:
        return "F20"_s;
    case Qt::Key_F21:
        return "F21"_s;
    case Qt::Key_F22:
        return "F22"_s;
    case Qt::Key_F23:
        return "F23"_s;
    case Qt::Key_F24:
        return "F24"_s;
    // 'LaunchApplication1'
    // 'LaunchApplication2'
    case Qt::Key_LaunchMail:
        return "LaunchMail"_s;
    // 'List'
    // 'Props'
    // 'Soft1'
    // 'Soft2'
    // 'Soft3'
    // 'Soft4'
    case Qt::Key_Yes:
        return "Accept"_s;
    // 'Again'
    case Qt::Key_Return:
    case Qt::Key_Enter:
        return "Enter"_s;
    case Qt::Key_Help:
        return "Help"_s;
    case Qt::Key_Menu:
    case Qt::Key_MenuKB:
        return "Menu"_s;
    case Qt::Key_Pause:
        return "Pause"_s;
    case Qt::Key_Play:
        return "Play"_s;
    case Qt::Key_Execute:
        return "Execute"_s;
    case Qt::Key_Cancel:
        return "Cancel"_s;
    case Qt::Key_Escape:
        return "Esc"_s;
    case Qt::Key_Zoom:
        return "Zoom"_s;
    case Qt::Key_Comma:
        return "Separator"_s;
    case Qt::Key_Plus:
        return "Add"_s;
    case Qt::Key_Minus:
        return "Subtract"_s;
    case Qt::Key_Asterisk:
        return "Multiply"_s;
    case Qt::Key_Slash:
        return "Divide"_s;
    case Qt::Key_Equal:
        return "Equals"_s;
    case Qt::Key_Period:
        return "Decimal"_s;
    case Qt::Key_MonBrightnessDown:
        return "BrightnessDown"_s;
    case Qt::Key_MonBrightnessUp:
        return "BrightnessUp"_s;
    case Qt::Key_Camera:
        return "Camera"_s;
    case Qt::Key_Eject:
        return "Eject"_s;
    case Qt::Key_PowerDown:
    case Qt::Key_PowerOff:
        return "Power"_s;
    case Qt::Key_Print:
        return "PrintScreen"_s;
    case Qt::Key_Favorites:
        return "BrowserFavorites"_s;
    case Qt::Key_HomePage:
        return "BrowserHome"_s;
    case Qt::Key_Refresh:
        return "BrowserRefresh"_s;
    case Qt::Key_Search:
        return "BrowserSearch"_s;
    case Qt::Key_Stop:
        return "BrowserStop"_s;
    case Qt::Key_Back:
        return "BrowserBack"_s;
    case Qt::Key_Forward:
        return "BrowserForward"_s;
    case Qt::Key_Left:
        return "Left"_s;
    case Qt::Key_PageDown:
        return "PageDown"_s;
    case Qt::Key_PageUp:
        return "PageUp"_s;
    case Qt::Key_Right:
        return "Right"_s;
    case Qt::Key_Up:
        return "Up"_s;
    // 'UpLeft'
    // 'UpRight'
    case Qt::Key_Down:
        return "Down"_s;
    // 'DownLeft'
    // 'DownRight'
    case Qt::Key_Home:
        return "Home"_s;
    case Qt::Key_End:
        return "End"_s;
    case Qt::Key_Select:
        return "Select"_s;
    case Qt::Key_Clear:
        return "Clear"_s;
    case Qt::Key_Copy:
        return "Copy"_s;
    case Qt::Key_Cut:
        return "Cut"_s;
    // 'EraseEof'
    case Qt::Key_Insert:
        return "Insert"_s;
    case Qt::Key_Paste:
        return "Paste"_s;
    case Qt::Key_Dead_Grave:
        return "DeadGrave"_s;
    case Qt::Key_Dead_Acute:
        return "DeadAcute"_s;
    case Qt::Key_Dead_Circumflex:
        return "DeadCircumflex"_s;
    case Qt::Key_Dead_Tilde:
        return "DeadTilde"_s;
    case Qt::Key_Dead_Macron:
        return "DeadMacron"_s;
    case Qt::Key_Dead_Breve:
        return "DeadBreve"_s;
    case Qt::Key_Dead_Abovedot:
        return "DeadAboveDot"_s;
    case Qt::Key_Dead_Diaeresis:
        return "DeadUmlaut"_s;
    case Qt::Key_Dead_Abovering:
        return "DeadAboveRing"_s;
    case Qt::Key_Dead_Doubleacute:
        return "DeadDoubleAcute"_s;
    case Qt::Key_Dead_Caron:
        return "DeadCaron"_s;
    case Qt::Key_Dead_Cedilla:
        return "DeadCedilla"_s;
    case Qt::Key_Dead_Ogonek:
        return "DeadOgonek"_s;
    case Qt::Key_Dead_Iota:
        return "DeadIota"_s;
    case Qt::Key_Dead_Voiced_Sound:
        return "DeadVoicedSound"_s;
    case Qt::Key_Dead_Semivoiced_Sound:
        return "DeadSemivoicedSound"_s;

    case Qt::Key_MultipleCandidate:
        return "AllCandidate"_s;
    case Qt::Key_SingleCandidate:
        return "NextCandidate"_s;
    case Qt::Key_PreviousCandidate:
        return "PreviousCandidate"_s;
    case Qt::Key_Codeinput:
        return "CodeInput"_s;

    case Qt::Key_Mode_switch:
        return "ModeChange"_s;
    case Qt::Key_Hangul:
        return "HangulMode"_s;
    case Qt::Key_Hangul_Hanja:
        return "HanjaMode"_s;
    case Qt::Key_Hiragana:
        return "Hiragana"_s;
    case Qt::Key_Kana_Lock:
    case Qt::Key_Kana_Shift:
        return "KanaMode"_s;
    case Qt::Key_Kanji:
        return "KanjiMode"_s;
    case Qt::Key_Katakana:
        return "Katakana"_s;

    // 'AudioFaderFront'
    // 'AudioFaderRear'
    // 'AudioBalanceLeft'
    // 'AudioBalanceRight'
    case Qt::Key_BassDown:
        return "AudioBassBoostDown"_s;
    case Qt::Key_BassUp:
        return "AudioBassBoostUp"_s;
    case Qt::Key_VolumeMute:
        return "VolumeMute"_s;
    case Qt::Key_VolumeDown:
        return "VolumeDown"_s;
    case Qt::Key_VolumeUp:
        return "VolumeUp"_s;
    case Qt::Key_MediaPause:
        return "MediaPause"_s;
    case Qt::Key_MediaPlay:
        return "MediaPlay"_s;
    // 'MediaTrackEnd'
    case Qt::Key_MediaNext:
        return "MediaNextTrack"_s;
    case Qt::Key_MediaTogglePlayPause:
        return "MediaPlayPause"_s;
    case Qt::Key_MediaPrevious:
        return "MediaPreviousTrack"_s;
    // 'MediaTrackSkip'
    // 'MediaTrackStart'
    case Qt::Key_MediaStop:
        return "MediaStop"_s;
    case Qt::Key_LaunchMedia:
        return "SelectMedia"_s;

    case Qt::Key_AudioForward:
        return "FastFwd"_s;
    case Qt::Key_MediaRecord:
        return "MediaRecord"_s;
    case Qt::Key_AudioRewind:
        return "MediaRewind"_s;
    case Qt::Key_Subtitle:
        return "Subtitle"_s;
    case Qt::Key_Blue:
        return "Blue"_s;
    case Qt::Key_ChannelDown:
        return "ChannelDown"_s;
    case Qt::Key_ChannelUp:
        return "ChannelUp"_s;
    case Qt::Key_Green:
        return "Green"_s;
    case Qt::Key_Red:
        return "Red"_s;
    case Qt::Key_Yellow:
        return "Yellow"_s;
#if QT_VERSION >= QT_VERSION_CHECK(5,4,0)
    case Qt::Key_Find:
        return "Find"_s;
    case Qt::Key_Info:
        return "Info"_s;
    case Qt::Key_Exit:
        return "Exit"_s;
    case Qt::Key_Undo:
        return "Undo"_s;
    case Qt::Key_Guide:
        return "Guide"_s;
    case Qt::Key_Settings:
        return "Settings"_s;
#endif
    // Keys we have returned U+charcode for in the past.
    // FIXME: Change them to correct standard values if others do.
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        return "U+0009"_s; // return "Tab"_s;
    case Qt::Key_Backspace:
        return "U+0008"_s; // return "Backspace"_s;
    case Qt::Key_Delete:
        return "U+007F"_s; // return "Del"_s;
    default:
        if (keyCode < 128)
            return makeString("U+", hex(toASCIIUpper(keyCode), 4));
        return String();
    }
}

int windowsKeyCodeForKeyEvent(int keycode, bool isKeypad)
{
    // Determine wheter the event comes from the keypad
    if (isKeypad) {
        switch (keycode) {
        case Qt::Key_0:
            return VK_NUMPAD0; // (60) Numeric keypad 0 key
        case Qt::Key_1:
            return VK_NUMPAD1; // (61) Numeric keypad 1 key
        case Qt::Key_2:
            return  VK_NUMPAD2; // (62) Numeric keypad 2 key
        case Qt::Key_3:
            return VK_NUMPAD3; // (63) Numeric keypad 3 key
        case Qt::Key_4:
            return VK_NUMPAD4; // (64) Numeric keypad 4 key
        case Qt::Key_5:
            return VK_NUMPAD5; // (65) Numeric keypad 5 key
        case Qt::Key_6:
            return VK_NUMPAD6; // (66) Numeric keypad 6 key
        case Qt::Key_7:
            return VK_NUMPAD7; // (67) Numeric keypad 7 key
        case Qt::Key_8:
            return VK_NUMPAD8; // (68) Numeric keypad 8 key
        case Qt::Key_9:
            return VK_NUMPAD9; // (69) Numeric keypad 9 key
        case Qt::Key_Asterisk:
            return VK_MULTIPLY; // (6A) Multiply key
        case Qt::Key_Plus:
            return VK_ADD; // (6B) Add key
        case Qt::Key_Minus:
            return VK_SUBTRACT; // (6D) Subtract key
        case Qt::Key_Period:
            return VK_DECIMAL; // (6E) Decimal key
        case Qt::Key_Slash:
            return VK_DIVIDE; // (6F) Divide key
        case Qt::Key_PageUp:
            return VK_PRIOR; // (21) PAGE UP key
        case Qt::Key_PageDown:
            return VK_NEXT; // (22) PAGE DOWN key
        case Qt::Key_End:
            return VK_END; // (23) END key
        case Qt::Key_Home:
            return VK_HOME; // (24) HOME key
        case Qt::Key_Left:
            return VK_LEFT; // (25) LEFT ARROW key
        case Qt::Key_Up:
            return VK_UP; // (26) UP ARROW key
        case Qt::Key_Right:
            return VK_RIGHT; // (27) RIGHT ARROW key
        case Qt::Key_Down:
            return VK_DOWN; // (28) DOWN ARROW key
        case Qt::Key_Enter:
        case Qt::Key_Return:
            return VK_RETURN; // (0D) Return key
        case Qt::Key_Insert:
            return VK_INSERT; // (2D) INS key
        case Qt::Key_Delete:
            return VK_DELETE; // (2E) DEL key
        default:
            return 0;
        }

    } else

    switch (keycode) {
    case Qt::Key_Backspace:
        return VK_BACK; // (08) BACKSPACE key
    case Qt::Key_Backtab:
    case Qt::Key_Tab:
        return VK_TAB; // (09) TAB key
    case Qt::Key_Clear:
        return VK_CLEAR; // (0C) CLEAR key
    case Qt::Key_Enter:
    case Qt::Key_Return:
        return VK_RETURN; // (0D) Return key
    case Qt::Key_Shift:
        return VK_SHIFT; // (10) SHIFT key
    case Qt::Key_Control:
        return VK_CONTROL; // (11) CTRL key
    case Qt::Key_Alt:
        return VK_MENU; // (12) ALT key

    case Qt::Key_F1:
        return VK_F1;
    case Qt::Key_F2:
        return VK_F2;
    case Qt::Key_F3:
        return VK_F3;
    case Qt::Key_F4:
        return VK_F4;
    case Qt::Key_F5:
        return VK_F5;
    case Qt::Key_F6:
        return VK_F6;
    case Qt::Key_F7:
        return VK_F7;
    case Qt::Key_F8:
        return VK_F8;
    case Qt::Key_F9:
        return VK_F9;
    case Qt::Key_F10:
        return VK_F10;
    case Qt::Key_F11:
        return VK_F11;
    case Qt::Key_F12:
        return VK_F12;
    case Qt::Key_F13:
        return VK_F13;
    case Qt::Key_F14:
        return VK_F14;
    case Qt::Key_F15:
        return VK_F15;
    case Qt::Key_F16:
        return VK_F16;
    case Qt::Key_F17:
        return VK_F17;
    case Qt::Key_F18:
        return VK_F18;
    case Qt::Key_F19:
        return VK_F19;
    case Qt::Key_F20:
        return VK_F20;
    case Qt::Key_F21:
        return VK_F21;
    case Qt::Key_F22:
        return VK_F22;
    case Qt::Key_F23:
        return VK_F23;
    case Qt::Key_F24:
        return VK_F24;

    case Qt::Key_Pause:
        return VK_PAUSE; // (13) PAUSE key
    case Qt::Key_CapsLock:
        return VK_CAPITAL; // (14) CAPS LOCK key
    case Qt::Key_Kana_Lock:
    case Qt::Key_Kana_Shift:
        return VK_KANA; // (15) Input Method Editor (IME) Kana mode
    case Qt::Key_Hangul:
        return VK_HANGUL; // VK_HANGUL (15) IME Hangul mode
        // VK_JUNJA (17) IME Junja mode
        // VK_FINAL (18) IME final mode
    case Qt::Key_Hangul_Hanja:
        return VK_HANJA; // (19) IME Hanja mode
    case Qt::Key_Kanji:
        return VK_KANJI; // (19) IME Kanji mode
    case Qt::Key_Escape:
        return VK_ESCAPE; // (1B) ESC key
        // VK_CONVERT (1C) IME convert
        // VK_NONCONVERT (1D) IME nonconvert
        // VK_ACCEPT (1E) IME accept
        // VK_MODECHANGE (1F) IME mode change request
    case Qt::Key_Space:
        return VK_SPACE; // (20) SPACEBAR
    case Qt::Key_PageUp:
        return VK_PRIOR; // (21) PAGE UP key
    case Qt::Key_PageDown:
        return VK_NEXT; // (22) PAGE DOWN key
    case Qt::Key_End:
        return VK_END; // (23) END key
    case Qt::Key_Home:
        return VK_HOME; // (24) HOME key
    case Qt::Key_Left:
        return VK_LEFT; // (25) LEFT ARROW key
    case Qt::Key_Up:
        return VK_UP; // (26) UP ARROW key
    case Qt::Key_Right:
        return VK_RIGHT; // (27) RIGHT ARROW key
    case Qt::Key_Down:
        return VK_DOWN; // (28) DOWN ARROW key
    case Qt::Key_Select:
        return VK_SELECT; // (29) SELECT key
    case Qt::Key_Print:
        return VK_SNAPSHOT; // (2A) PRINT key
    case Qt::Key_Execute:
        return VK_EXECUTE; // (2B) EXECUTE key
    case Qt::Key_Insert:
        return VK_INSERT; // (2D) INS key
    case Qt::Key_Delete:
        return VK_DELETE; // (2E) DEL key
    case Qt::Key_Help:
        return VK_HELP; // (2F) HELP key
    case Qt::Key_0:
    case Qt::Key_ParenRight:
        return VK_0; // (30) 0) key
    case Qt::Key_1:
    case Qt::Key_Exclam:
        return VK_1; // (31) 1 ! key
    case Qt::Key_2:
    case Qt::Key_At:
        return VK_2; // (32) 2 & key
    case Qt::Key_3:
    case Qt::Key_NumberSign:
        return VK_3; // case '3': case '#';
    case Qt::Key_4:
    case Qt::Key_Dollar: // (34) 4 key '$';
        return VK_4;
    case Qt::Key_5:
    case Qt::Key_Percent:
        return VK_5; // (35) 5 key  '%'
    case Qt::Key_6:
    case Qt::Key_AsciiCircum:
        return VK_6; // (36) 6 key  '^'
    case Qt::Key_7:
    case Qt::Key_Ampersand:
        return VK_7; // (37) 7 key  case '&'
    case Qt::Key_8:
    case Qt::Key_Asterisk:
        return VK_8; // (38) 8 key  '*'
    case Qt::Key_9:
    case Qt::Key_ParenLeft:
        return VK_9; // (39) 9 key '('
    case Qt::Key_A:
        return VK_A; // (41) A key case 'a': case 'A': return 0x41;
    case Qt::Key_B:
        return VK_B; // (42) B key case 'b': case 'B': return 0x42;
    case Qt::Key_C:
        return VK_C; // (43) C key case 'c': case 'C': return 0x43;
    case Qt::Key_D:
        return VK_D; // (44) D key case 'd': case 'D': return 0x44;
    case Qt::Key_E:
        return VK_E; // (45) E key case 'e': case 'E': return 0x45;
    case Qt::Key_F:
        return VK_F; // (46) F key case 'f': case 'F': return 0x46;
    case Qt::Key_G:
        return VK_G; // (47) G key case 'g': case 'G': return 0x47;
    case Qt::Key_H:
        return VK_H; // (48) H key case 'h': case 'H': return 0x48;
    case Qt::Key_I:
        return VK_I; // (49) I key case 'i': case 'I': return 0x49;
    case Qt::Key_J:
        return VK_J; // (4A) J key case 'j': case 'J': return 0x4A;
    case Qt::Key_K:
        return VK_K; // (4B) K key case 'k': case 'K': return 0x4B;
    case Qt::Key_L:
        return VK_L; // (4C) L key case 'l': case 'L': return 0x4C;
    case Qt::Key_M:
        return VK_M; // (4D) M key case 'm': case 'M': return 0x4D;
    case Qt::Key_N:
        return VK_N; // (4E) N key case 'n': case 'N': return 0x4E;
    case Qt::Key_O:
        return VK_O; // (4F) O key case 'o': case 'O': return 0x4F;
    case Qt::Key_P:
        return VK_P; // (50) P key case 'p': case 'P': return 0x50;
    case Qt::Key_Q:
        return VK_Q; // (51) Q key case 'q': case 'Q': return 0x51;
    case Qt::Key_R:
        return VK_R; // (52) R key case 'r': case 'R': return 0x52;
    case Qt::Key_S:
        return VK_S; // (53) S key case 's': case 'S': return 0x53;
    case Qt::Key_T:
        return VK_T; // (54) T key case 't': case 'T': return 0x54;
    case Qt::Key_U:
        return VK_U; // (55) U key case 'u': case 'U': return 0x55;
    case Qt::Key_V:
        return VK_V; // (56) V key case 'v': case 'V': return 0x56;
    case Qt::Key_W:
        return VK_W; // (57) W key case 'w': case 'W': return 0x57;
    case Qt::Key_X:
        return VK_X; // (58) X key case 'x': case 'X': return 0x58;
    case Qt::Key_Y:
        return VK_Y; // (59) Y key case 'y': case 'Y': return 0x59;
    case Qt::Key_Z:
        return VK_Z; // (5A) Z key case 'z': case 'Z': return 0x5A;
    case Qt::Key_Meta:
        return VK_LWIN; // (5B) Left Windows key (Microsoft Natural keyboard)
        // case Qt::Key_Meta_R: FIXME: What to do here?
        //    return VK_RWIN; // (5C) Right Windows key (Natural keyboard)
    case Qt::Key_Menu:
        return VK_APPS; // (5D) Applications key (Natural keyboard)
        // VK_SLEEP (5F) Computer Sleep key
        // VK_SEPARATOR (6C) Separator key
        // VK_SUBTRACT (6D) Subtract key
        // VK_DECIMAL (6E) Decimal key
        // VK_DIVIDE (6F) Divide key
        // handled by key code above

    case Qt::Key_NumLock:
        return VK_NUMLOCK; // (90) NUM LOCK key

    case Qt::Key_ScrollLock:
        return VK_SCROLL; // (91) SCROLL LOCK key

        // VK_LSHIFT (A0) Left SHIFT key
        // VK_RSHIFT (A1) Right SHIFT key
        // VK_LCONTROL (A2) Left CONTROL key
        // VK_RCONTROL (A3) Right CONTROL key
        // VK_LMENU (A4) Left MENU key
        // VK_RMENU (A5) Right MENU key
        // VK_BROWSER_BACK (A6) Windows 2000/XP: Browser Back key
        // VK_BROWSER_FORWARD (A7) Windows 2000/XP: Browser Forward key
        // VK_BROWSER_REFRESH (A8) Windows 2000/XP: Browser Refresh key
        // VK_BROWSER_STOP (A9) Windows 2000/XP: Browser Stop key
        // VK_BROWSER_SEARCH (AA) Windows 2000/XP: Browser Search key
        // VK_BROWSER_FAVORITES (AB) Windows 2000/XP: Browser Favorites key
        // VK_BROWSER_HOME (AC) Windows 2000/XP: Browser Start and Home key

    case Qt::Key_VolumeMute:
        return VK_VOLUME_MUTE; // (AD) Windows 2000/XP: Volume Mute key
    case Qt::Key_VolumeDown:
        return VK_VOLUME_DOWN; // (AE) Windows 2000/XP: Volume Down key
    case Qt::Key_VolumeUp:
        return VK_VOLUME_UP; // (AF) Windows 2000/XP: Volume Up key
    case Qt::Key_MediaNext:
        return VK_MEDIA_NEXT_TRACK; // (B0) Windows 2000/XP: Next Track key
    case Qt::Key_MediaPrevious:
        return VK_MEDIA_PREV_TRACK; // (B1) Windows 2000/XP: Previous Track key
    case Qt::Key_MediaStop:
        return VK_MEDIA_STOP; // (B2) Windows 2000/XP: Stop Media key
    case Qt::Key_MediaTogglePlayPause:
        return VK_MEDIA_PLAY_PAUSE; // (B3) Windows 2000/XP: Play/Pause Media key

        // VK_LAUNCH_MAIL (B4) Windows 2000/XP: Start Mail key
        // VK_LAUNCH_MEDIA_SELECT (B5) Windows 2000/XP: Select Media key
        // VK_LAUNCH_APP1 (B6) Windows 2000/XP: Start Application 1 key
        // VK_LAUNCH_APP2 (B7) Windows 2000/XP: Start Application 2 key

        // VK_OEM_1 (BA) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ';:' key
    case Qt::Key_Semicolon:
    case Qt::Key_Colon:
        return VK_OEM_1; // case ';': case ':': return 0xBA;
        // VK_OEM_PLUS (BB) Windows 2000/XP: For any country/region, the '+' key
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        return VK_OEM_PLUS; // case '=': case '+': return 0xBB;
        // VK_OEM_COMMA (BC) Windows 2000/XP: For any country/region, the ',' key
    case Qt::Key_Comma:
    case Qt::Key_Less:
        return VK_OEM_COMMA; // case ',': case '<': return 0xBC;
        // VK_OEM_MINUS (BD) Windows 2000/XP: For any country/region, the '-' key
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        return VK_OEM_MINUS; // case '-': case '_': return 0xBD;
        // VK_OEM_PERIOD (BE) Windows 2000/XP: For any country/region, the '.' key
    case Qt::Key_Period:
    case Qt::Key_Greater:
        return VK_OEM_PERIOD; // case '.': case '>': return 0xBE;
        // VK_OEM_2 (BF) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '/?' key
    case Qt::Key_Slash:
    case Qt::Key_Question:
        return VK_OEM_2; // case '/': case '?': return 0xBF;
        // VK_OEM_3 (C0) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '`~' key
    case Qt::Key_AsciiTilde:
    case Qt::Key_QuoteLeft:
        return VK_OEM_3; // case '`': case '~': return 0xC0;
        // VK_OEM_4 (DB) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '[{' key
    case Qt::Key_BracketLeft:
    case Qt::Key_BraceLeft:
        return VK_OEM_4; // case '[': case '{': return 0xDB;
        // VK_OEM_5 (DC) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the '\|' key
    case Qt::Key_Backslash:
    case Qt::Key_Bar:
        return VK_OEM_5; // case '\\': case '|': return 0xDC;
        // VK_OEM_6 (DD) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the ']}' key
    case Qt::Key_BracketRight:
    case Qt::Key_BraceRight:
        return VK_OEM_6; // case ']': case '}': return 0xDD;
        // VK_OEM_7 (DE) Used for miscellaneous characters; it can vary by keyboard. Windows 2000/XP: For the US standard keyboard, the 'single-quote/double-quote' key
    case Qt::Key_Apostrophe:
    case Qt::Key_QuoteDbl:
        return VK_OEM_7; // case '\'': case '"': return 0xDE;
        // VK_OEM_8 (DF) Used for miscellaneous characters; it can vary by keyboard.
        // VK_OEM_102 (E2) Windows 2000/XP: Either the angle bracket key or the backslash key on the RT 102-key keyboard

    case Qt::Key_AudioRewind:
        return 0xE3; // (E3) Android/GoogleTV: Rewind media key (Windows: VK_ICO_HELP Help key on 1984 Olivetti M24 deluxe keyboard)
    case Qt::Key_AudioForward:
        return 0xE4; // (E4) Android/GoogleTV: Fast forward media key  (Windows: VK_ICO_00 '00' key on 1984 Olivetti M24 deluxe keyboard)

        // VK_PROCESSKEY (E5) Windows 95/98/Me, Windows NT 4.0, Windows 2000/XP: IME PROCESS key
        // VK_PACKET (E7) Windows 2000/XP: Used to pass Unicode characters as if they were keystrokes. The VK_PACKET key is the low word of a 32-bit Virtual Key value used for non-keyboard input methods. For more information, see Remark in KEYBDINPUT,SendInput, WM_KEYDOWN, and WM_KEYUP
        // VK_ATTN (F6) Attn key
        // VK_CRSEL (F7) CrSel key
        // VK_EXSEL (F8) ExSel key
        // VK_EREOF (F9) Erase EOF key
        // VK_PLAY (FA) Play key
        // VK_ZOOM (FB) Zoom key
        // VK_NONAME (FC) Reserved for future use
        // VK_PA1 (FD) PA1 key
        // VK_OEM_CLEAR (FE) Clear key
    default:
        return 0;
    }
}

static bool isVirtualKeyCodeRepresentingCharacter(int code)
{
    switch (code) {
    case VK_SPACE:
    case VK_0:
    case VK_1:
    case VK_2:
    case VK_3:
    case VK_4:
    case VK_5:
    case VK_6:
    case VK_7:
    case VK_8:
    case VK_9:
    case VK_A:
    case VK_B:
    case VK_C:
    case VK_D:
    case VK_E:
    case VK_F:
    case VK_G:
    case VK_H:
    case VK_I:
    case VK_J:
    case VK_K:
    case VK_L:
    case VK_M:
    case VK_N:
    case VK_O:
    case VK_P:
    case VK_Q:
    case VK_R:
    case VK_S:
    case VK_T:
    case VK_U:
    case VK_V:
    case VK_W:
    case VK_X:
    case VK_Y:
    case VK_Z:
    case VK_NUMPAD0:
    case VK_NUMPAD1:
    case VK_NUMPAD2:
    case VK_NUMPAD3:
    case VK_NUMPAD4:
    case VK_NUMPAD5:
    case VK_NUMPAD6:
    case VK_NUMPAD7:
    case VK_NUMPAD8:
    case VK_NUMPAD9:
    case VK_MULTIPLY:
    case VK_ADD:
    case VK_SEPARATOR:
    case VK_SUBTRACT:
    case VK_DECIMAL:
    case VK_DIVIDE:
    case VK_OEM_1:
    case VK_OEM_PLUS:
    case VK_OEM_COMMA:
    case VK_OEM_MINUS:
    case VK_OEM_PERIOD:
    case VK_OEM_2:
    case VK_OEM_3:
    case VK_OEM_4:
    case VK_OEM_5:
    case VK_OEM_6:
    case VK_OEM_7:
        return true;
    default:
        return false;
    }
}

String keyCodeForQtKeyEvent(const QKeyEvent* event)
{
    switch (event->key()) {
#define MAKE_CODE_FOR_KEY(QtKey, Character) \
    case Qt::Key_##QtKey: \
        return #Character##_s; \
        break;

    MAKE_CODE_FOR_KEY(Escape, Escape);
    MAKE_CODE_FOR_KEY(1, Digit1);
    MAKE_CODE_FOR_KEY(A, KeyA);
    MAKE_CODE_FOR_KEY(B, KeyB);
// FIXME: Finish this...

#undef MAKE_CODE_FOR_KEY
    }
    return "Unidentified"_s;
}

template<bool unmodified>
static String keyTextForKeyEvent(const QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        if (event->text().isNull())
            return "\t"_s;
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (event->text().isNull())
            return "\r"_s;
        break;

// Workaround for broken accesskey when QKeyEvent has modifier, see QTBUG-64891
#define MAKE_TEXT_FOR_KEY(QtKey, Character) \
    case Qt::Key_##QtKey: \
        if (unmodified && event->text().isNull()) \
            return #Character##_s; \
        break;

    MAKE_TEXT_FOR_KEY(0, 0);
    MAKE_TEXT_FOR_KEY(1, 1);
    MAKE_TEXT_FOR_KEY(2, 2);
    MAKE_TEXT_FOR_KEY(3, 3);
    MAKE_TEXT_FOR_KEY(4, 4);
    MAKE_TEXT_FOR_KEY(5, 5);
    MAKE_TEXT_FOR_KEY(6, 6);
    MAKE_TEXT_FOR_KEY(7, 7);
    MAKE_TEXT_FOR_KEY(8, 8);
    MAKE_TEXT_FOR_KEY(9, 9);
    MAKE_TEXT_FOR_KEY(A, a);
    MAKE_TEXT_FOR_KEY(B, b);
    MAKE_TEXT_FOR_KEY(C, c);
    MAKE_TEXT_FOR_KEY(D, d);
    MAKE_TEXT_FOR_KEY(E, e);
    MAKE_TEXT_FOR_KEY(F, f);
    MAKE_TEXT_FOR_KEY(G, g);
    MAKE_TEXT_FOR_KEY(H, h);
    MAKE_TEXT_FOR_KEY(I, i);
    MAKE_TEXT_FOR_KEY(J, j);
    MAKE_TEXT_FOR_KEY(K, k);
    MAKE_TEXT_FOR_KEY(L, l);
    MAKE_TEXT_FOR_KEY(M, m);
    MAKE_TEXT_FOR_KEY(N, n);
    MAKE_TEXT_FOR_KEY(O, o);
    MAKE_TEXT_FOR_KEY(P, p);
    MAKE_TEXT_FOR_KEY(Q, q);
    MAKE_TEXT_FOR_KEY(R, r);
    MAKE_TEXT_FOR_KEY(S, s);
    MAKE_TEXT_FOR_KEY(T, t);
    MAKE_TEXT_FOR_KEY(U, u);
    MAKE_TEXT_FOR_KEY(V, v);
    MAKE_TEXT_FOR_KEY(W, w);
    MAKE_TEXT_FOR_KEY(X, x);
    MAKE_TEXT_FOR_KEY(Y, y);
    MAKE_TEXT_FOR_KEY(Z, z);

#undef MAKE_TEXT_FOR_KEY
    }
    return event->text();
}

template<typename ToType, typename FromType>
inline ToType safeCastOrNull(FromType value)
{
    return WTF::isInBounds<ToType>(value) ? static_cast<ToType>(value) : 0;
}

PlatformKeyboardEvent::PlatformKeyboardEvent(QKeyEvent* event, bool useNativeVirtualKeyAsDOMKey)
{
    const auto state = event->modifiers();
    m_type = (event->type() == QEvent::KeyRelease) ? PlatformEvent::KeyUp : PlatformEvent::KeyDown;

    if ((state & Qt::ShiftModifier) || event->key() == Qt::Key_Backtab) // Simulate Shift+Tab with Key_Backtab
        m_modifiers.add(Modifier::ShiftKey);
    if (state & Qt::ControlModifier)
        m_modifiers.add(Modifier::ControlKey);
    if (state & Qt::AltModifier)
        m_modifiers.add(Modifier::AltKey);
    if (state & Qt::MetaModifier)
        m_modifiers.add(Modifier::MetaKey);

    m_useNativeVirtualKeyAsDOMKey = useNativeVirtualKeyAsDOMKey;
    m_text = keyTextForKeyEvent<false>(event);
    m_unmodifiedText = keyTextForKeyEvent<true>(event);
    m_keyIdentifier = keyIdentifierForQtKeyCode(event->key());
    m_autoRepeat = event->isAutoRepeat();
    m_isKeypad = (state & Qt::KeypadModifier);
    m_isSystemKey = false;
    int nativeVirtualKeyCode = safeCastOrNull<int>(event->nativeVirtualKey());
    // If QKeyEvent::nativeVirtualKey() is valid (!=0) and useNativeVirtualKeyAsDOMKey is set,
    // then it is a special case desired by QtWebKit embedder to send domain specific keys
    // to Web Applications intented for platforms like HbbTV,CE-HTML,OIPF,..etc.
    if (useNativeVirtualKeyAsDOMKey && nativeVirtualKeyCode)
        m_windowsVirtualKeyCode = nativeVirtualKeyCode;
    else
        m_windowsVirtualKeyCode = windowsKeyCodeForKeyEvent(event->key(), m_isKeypad);

    m_qtEvent = event;
    m_timestamp = WTF::WallTime::now();
}

void PlatformKeyboardEvent::disambiguateKeyDownEvent(Type type, bool)
{
    // Can only change type from KeyDown to RawKeyDown or Char, as we lack information for other conversions.
    ASSERT(m_type == PlatformEvent::KeyDown);
    m_type = type;

    if (type == PlatformEvent::RawKeyDown) {
        m_text = String();
        m_unmodifiedText = String();
    } else {
        /*
            When we receive shortcut events like Ctrl+V then the text in the QKeyEvent is
            empty. If we're asked to disambiguate the event into a Char keyboard event,
            we try to detect this situation and still set the text, to ensure that the
            general event handling sends a key press event after this disambiguation.
        */
        if (!m_useNativeVirtualKeyAsDOMKey && m_text.isEmpty() && m_windowsVirtualKeyCode && isVirtualKeyCodeRepresentingCharacter(m_windowsVirtualKeyCode))
            m_text.append(UChar(m_windowsVirtualKeyCode));

        m_keyIdentifier = String();
        m_windowsVirtualKeyCode = 0;
    }
}

bool PlatformKeyboardEvent::currentCapsLockState()
{
    notImplemented();
    return false;
}

void PlatformKeyboardEvent::getCurrentModifierState(bool& shiftKey, bool& ctrlKey, bool& altKey, bool& metaKey)
{
    notImplemented();
    shiftKey = false;
    ctrlKey = false;
    altKey = false;
    metaKey = false;
}

uint32_t PlatformKeyboardEvent::nativeModifiers() const
{
    ASSERT(m_qtEvent);
    return m_qtEvent->nativeModifiers();
}

uint32_t PlatformKeyboardEvent::nativeScanCode() const
{
    ASSERT(m_qtEvent);
    return m_qtEvent->nativeScanCode();
}

}

// vim: ts=4 sw=4 et
