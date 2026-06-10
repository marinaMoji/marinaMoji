// Copyright 2010-2021, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#import <Cocoa/Cocoa.h>

namespace mozc {
namespace commands {
class KeyEvent;
}  // namespace commands
}  // namespace mozc

enum InputMode { ASCII, KANA };

@interface KeyCodeMap : NSObject {
 @private
  // |modifierFlags_| stores the current modifiers
  NSUInteger modifierFlags_;
  InputMode inputMode_;
  // Right Shift alone → ToggleManyoshuHiragana (Linux IBUS_Shift_R parity).
  BOOL rightShiftDown_;
  BOOL typedWhileRightShiftDown_;
  BOOL otherModifiersWhileRightShift_;
  // Left Shift alone → ToggleLeftShiftDirect (Linux IBUS_Shift_L parity).
  BOOL leftShiftDown_;
  BOOL typedWhileLeftShiftDown_;
  BOOL otherModifiersWhileLeftShift_;
  BOOL ctrlHeldDuringLeftShiftPress_;
  // Ctrl+Left Shift alone → ToggleLeftShiftModeLock.
  BOOL leftShiftPhysicallyDown_;
  BOOL ctrlPhysicallyDown_;
  BOOL ctrlLeftShiftChordArmed_;
  BOOL typedDuringCtrlLeftShiftChord_;
}
@property(assign, nonatomic) InputMode inputMode;

// Returns YES if |event| is a special key of mode switching
// a.k.a. Eisu or Kana key.
- (BOOL)isModeSwitchingKey:(NSEvent *)event;

// Extracts key event information from |event| and sets the extracted
// data into |keyEvent|.  Returns YES if the extraction succeeds.
// Otherwise, returns NO.
- (BOOL)getMozcKeyCodeFromKeyEvent:(NSEvent *)event
                    toMozcKeyEvent:(mozc::commands::KeyEvent *)keyEvent;

// FlagsChanged on kVK_RightShift: on release without typing, sets RIGHT_SHIFT only.
- (BOOL)tryRightShiftAloneKeyFromEvent:(NSEvent *)event
                        toMozcKeyEvent:(mozc::commands::KeyEvent *)keyEvent;

// FlagsChanged on kVK_Shift (left): on release without typing, sets LEFT_SHIFT only.
- (BOOL)tryLeftShiftAloneKeyFromEvent:(NSEvent *)event
                       toMozcKeyEvent:(mozc::commands::KeyEvent *)keyEvent;

// Ctrl+Left Shift chord released without typing: sets CTRL+LEFT_SHIFT.
- (BOOL)tryCtrlLeftShiftModeLockFromEvent:(NSEvent *)event
                           toMozcKeyEvent:(mozc::commands::KeyEvent *)keyEvent;
@end
