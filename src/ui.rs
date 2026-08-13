//! Minimal Win32 UI: a semi-transparent, click-through top-left panel that
//! shows the current high-arc distance in large digits.
//!
//! Zero external GUI dependencies: raw FFI to user32/gdi32/winmm only.

#![allow(non_camel_case_types)]
#![allow(dead_code)]

use std::ffi::c_void;
use std::ptr;

type HWND = *mut c_void;
type HDC = *mut c_void;
type HINSTANCE = *mut c_void;
type HICON = *mut c_void;
type HCURSOR = *mut c_void;
type HBRUSH = *mut c_void;
type HFONT = *mut c_void;
type HPEN = *mut c_void;
type HGDIOBJ = *mut c_void;
type LPCWSTR = *const u16;
type WNDPROC = Option<unsafe extern "system" fn(HWND, u32, usize, isize) -> isize>;

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct POINT {
    x: i32,
    y: i32,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
struct RECT {
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct MSG {
    pub hwnd: HWND,
    pub message: u32,
    pub wparam: usize,
    pub lparam: isize,
    pub time: u32,
    pub pt: POINT,
}

#[repr(C)]
struct PAINTSTRUCT {
    hdc: HDC,
    f_erase: i32,
    rc_paint: RECT,
    f_restore: i32,
    f_inc_update: i32,
    rgb_reserved: [u8; 32],
}

#[repr(C)]
struct WNDCLASSEXW {
    cb_size: u32,
    style: u32,
    lpfn_wnd_proc: WNDPROC,
    cb_cls_extra: i32,
    cb_wnd_extra: i32,
    h_instance: HINSTANCE,
    h_icon: HICON,
    h_cursor: HCURSOR,
    hbr_background: HBRUSH,
    lpsz_menu_name: LPCWSTR,
    lpsz_class_name: LPCWSTR,
    h_icon_sm: HICON,
}

#[link(name = "user32")]
extern "system" {
    fn RegisterClassExW(class: *const WNDCLASSEXW) -> u16;
    fn CreateWindowExW(
        ex_style: u32,
        class: LPCWSTR,
        name: LPCWSTR,
        style: u32,
        x: i32,
        y: i32,
        w: i32,
        h: i32,
        parent: HWND,
        menu: HWND,
        instance: HINSTANCE,
        param: *const c_void,
    ) -> HWND;
    fn DefWindowProcW(hwnd: HWND, msg: u32, wp: usize, lp: isize) -> isize;
    fn ShowWindow(hwnd: HWND, cmd: i32) -> i32;
    fn UpdateWindow(hwnd: HWND) -> i32;
    fn InvalidateRect(hwnd: HWND, rect: *const RECT, erase: i32) -> i32;
    fn SetWindowPos(
        hwnd: HWND,
        after: HWND,
        x: i32,
        y: i32,
        w: i32,
        h: i32,
        flags: u32,
    ) -> i32;
    fn SetLayeredWindowAttributes(hwnd: HWND, color: u32, alpha: u8, flags: u32) -> i32;
    fn RegisterHotKey(hwnd: HWND, id: i32, modifiers: u32, vk: u32) -> i32;
    fn UnregisterHotKey(hwnd: HWND, id: i32) -> i32;
    fn GetMessageW(msg: *mut MSG, hwnd: HWND, min: u32, max: u32) -> i32;
    pub fn PeekMessageW(msg: *mut MSG, hwnd: HWND, min: u32, max: u32, remove: u32) -> i32;
    pub fn TranslateMessage(msg: *const MSG) -> i32;
    pub fn DispatchMessageW(msg: *const MSG) -> isize;
    fn PostQuitMessage(code: i32);
    fn BeginPaint(hwnd: HWND, paint: *mut PAINTSTRUCT) -> HDC;
    fn EndPaint(hwnd: HWND, paint: *const PAINTSTRUCT) -> i32;
    fn GetClientRect(hwnd: HWND, rect: *mut RECT) -> i32;
    fn DrawTextW(hdc: HDC, text: LPCWSTR, len: i32, rect: *mut RECT, fmt: u32) -> i32;
    fn SystemParametersInfoW(action: u32, param: u32, data: *mut c_void, fwinini: u32) -> i32;
    fn LoadCursorW(instance: HINSTANCE, name: LPCWSTR) -> HCURSOR;
    fn SetWindowLongPtrW(hwnd: HWND, index: i32, value: isize) -> isize;
    fn GetWindowLongPtrW(hwnd: HWND, index: i32) -> isize;
    fn DestroyWindow(hwnd: HWND) -> i32;
}

#[link(name = "gdi32")]
extern "system" {
    fn CreateSolidBrush(color: u32) -> HBRUSH;
    fn CreatePen(style: i32, width: i32, color: u32) -> HPEN;
    fn SelectObject(dc: HDC, obj: HGDIOBJ) -> HGDIOBJ;
    fn DeleteObject(obj: HGDIOBJ) -> i32;
    fn GetStockObject(kind: i32) -> HGDIOBJ;
    fn FillRect(dc: HDC, rect: *const RECT, brush: HBRUSH) -> i32;
    fn SetBkMode(dc: HDC, mode: i32) -> i32;
    fn SetTextColor(dc: HDC, color: u32) -> u32;
    fn CreateFontW(
        height: i32,
        width: i32,
        esc: i32,
        orient: i32,
        weight: i32,
        italic: u32,
        underline: u32,
        strike: u32,
        charset: u32,
        out_prec: u32,
        clip_prec: u32,
        quality: u32,
        pitch: u32,
        face: LPCWSTR,
    ) -> HFONT;
    fn Rectangle(dc: HDC, left: i32, top: i32, right: i32, bottom: i32) -> i32;
}

#[link(name = "winmm")]
extern "system" {
    pub fn timeBeginPeriod(period: u32) -> u32;
    pub fn timeEndPeriod(period: u32) -> u32;
}

#[link(name = "kernel32")]
extern "system" {
    pub fn GetModuleHandleW(module: LPCWSTR) -> HINSTANCE;
    fn GetModuleFileNameW(module: HINSTANCE, file: *mut u16, size: u32) -> u32;
}

// Constants
const GWLP_USERDATA: i32 = -21;
const WS_POPUP: u32 = 0x8000_0000;
const WS_EX_TOPMOST: u32 = 0x0000_0008;
const WS_EX_TOOLWINDOW: u32 = 0x0000_0080;
const WS_EX_NOACTIVATE: u32 = 0x0800_0000;
const WS_EX_LAYERED: u32 = 0x0008_0000;
const WS_EX_TRANSPARENT: u32 = 0x0000_0020;
const SW_SHOWNOACTIVATE: i32 = 4;
const SW_HIDE: i32 = 0;
const SWP_NOACTIVATE: u32 = 0x0010;
const SWP_SHOWWINDOW: u32 = 0x0040;
const SWP_HIDEWINDOW: u32 = 0x0080;
const LWA_ALPHA: u32 = 0x0002;
const WM_PAINT: u32 = 0x000F;
const WM_NCHITTEST: u32 = 0x0084;
const WM_DESTROY: u32 = 0x0002;
const WM_HOTKEY: u32 = 0x0312;
const WM_DISPLAYCHANGE: u32 = 0x007E;
const WM_ERASEBKGND: u32 = 0x0014;
pub const WM_QUIT: u32 = 0x0012;
const HTTRANSPARENT: isize = -1;
const VK_F9: u32 = 0x78;
const MOD_CONTROL: u32 = 0x0002;
const MOD_SHIFT: u32 = 0x0004;
const MOD_NOREPEAT: u32 = 0x4000;
const CS_HREDRAW: u32 = 0x0002;
const CS_VREDRAW: u32 = 0x0001;
const SPI_GETWORKAREA: u32 = 0x0030;
pub const PM_REMOVE: u32 = 0x0001;
const FW_BOLD: i32 = 700;
const DEFAULT_CHARSET: u32 = 1;
const OUT_DEFAULT_PRECIS: u32 = 0;
const CLIP_DEFAULT_PRECIS: u32 = 0;
const CLEARTYPE_QUALITY: u32 = 5;
const DEFAULT_PITCH: u32 = 0;
const TRANSPARENT_BK: i32 = 1;
const DT_SINGLELINE: u32 = 0x20;
const DT_VCENTER: u32 = 0x04;
const DT_LEFT: u32 = 0x00;
const DT_NOCLIP: u32 = 0x100;
const BLACK_BRUSH: i32 = 4;
const HOLLOW_BRUSH: i32 = 5;
const PS_SOLID: i32 = 0;

const HOTKEY_TOGGLE: i32 = 1;
const HOTKEY_QUIT: i32 = 2;

const WINDOW_W: i32 = 260;
const WINDOW_H: i32 = 100;
const WINDOW_X: i32 = 12;
const WINDOW_Y: i32 = 96;

/// UI state shown by the panel.
#[derive(Clone, Copy, Debug, Default)]
pub struct PanelState {
    pub pitch: Option<f64>,
    pub distance: Option<f64>,
    pub confidence: f64,
    pub capture_ok: bool,
}

pub struct Window {
    hwnd: HWND,
    instance: HINSTANCE,
    big_font: HFONT,
    small_font: HFONT,
    bg_brush: HBRUSH,
    border_pen: HPEN,
    visible: bool,
    state: PanelState,
    shown: PanelState, // last painted
    top: i32,
    pub quit: bool,
}

impl Window {
    pub fn new(instance: HINSTANCE) -> Result<Box<Self>, String> {
        let class_ptr = widestring("ApexAngleReaderOverlay");

        let mut wc: WNDCLASSEXW = unsafe { std::mem::zeroed() };
        wc.cb_size = std::mem::size_of::<WNDCLASSEXW>() as u32;
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfn_wnd_proc = Some(window_proc);
        wc.h_instance = instance;
        // IDC_ARROW = MAKEINTRESOURCE(32512)
        wc.h_cursor = unsafe { LoadCursorW(ptr::null_mut(), 32512usize as *const u16) };
        wc.hbr_background = unsafe { GetStockObject(BLACK_BRUSH) } as HBRUSH;
        wc.lpsz_class_name = class_ptr.as_ptr();
        let atom = unsafe { RegisterClassExW(&wc) };
        if atom == 0 {
            return Err("RegisterClassExW failed".into());
        }

        let hwnd = unsafe {
            CreateWindowExW(
                WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED
                    | WS_EX_TRANSPARENT,
                class_ptr.as_ptr(),
                widestring("Apex Angle Reader").as_ptr(),
                WS_POPUP,
                WINDOW_X,
                WINDOW_Y,
                WINDOW_W,
                WINDOW_H,
                ptr::null_mut(),
                ptr::null_mut(),
                instance,
                ptr::null(),
            )
        };
        if hwnd.is_null() {
            return Err("CreateWindowExW failed".into());
        }
        // Semi-transparent (alpha ~80%) so game data behind stays readable.
        unsafe {
            SetLayeredWindowAttributes(hwnd, 0, 205, LWA_ALPHA);
        }

        let big_font = unsafe {
            CreateFontW(
                -48, 0, 0, 0, FW_BOLD, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                widestring("Consolas").as_ptr(),
            )
        };
        let small_font = unsafe {
            CreateFontW(
                -15, 0, 0, 0, 400, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH,
                widestring("Microsoft YaHei UI").as_ptr(),
            )
        };
        let bg_brush = unsafe { CreateSolidBrush(0x1310_0A) }; // RGB(10,16,19) little-endian
        let border_pen = unsafe { CreatePen(PS_SOLID, 2, 0xAA78_5A) }; // RGB(90,120,170)
        if big_font.is_null() || small_font.is_null() || bg_brush.is_null()
            || border_pen.is_null()
        {
            return Err("GDI object creation failed".into());
        }

        let mut win = Box::new(Window {
            hwnd,
            instance,
            big_font,
            small_font,
            bg_brush,
            border_pen,
            visible: true,
            state: PanelState::default(),
            shown: PanelState::default(),
            top: WINDOW_Y,
            quit: false,
        });
        unsafe {
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, &mut *win as *mut Window as isize);
            RegisterHotKey(hwnd, HOTKEY_TOGGLE, 0, VK_F9);
            RegisterHotKey(hwnd, HOTKEY_QUIT, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x51);
            ShowWindow(hwnd, SW_SHOWNOACTIVATE);
            UpdateWindow(hwnd);
        }
        Ok(win)
    }

    pub fn set_state(&mut self, state: PanelState) {
        self.state = state;
    }

    pub fn visible(&self) -> bool {
        self.visible
    }

    /// Redraw only when the displayed content changed.
    pub fn present(&mut self) {
        let changed = self.state.pitch != self.shown.pitch
            || self.state.capture_ok != self.shown.capture_ok
            || self.state.confidence != self.shown.confidence;
        if !changed {
            return;
        }
        self.shown = self.state;
        unsafe {
            InvalidateRect(self.hwnd, ptr::null(), 0);
            UpdateWindow(self.hwnd);
        }
    }

    /// Override the panel top offset (physical pixels from the work-area top).
    pub fn set_top(&mut self, top: usize) {
        self.top = top as i32;
        self.position();
    }

    pub fn destroy(&mut self) {
        unsafe {
            UnregisterHotKey(self.hwnd, HOTKEY_TOGGLE);
            UnregisterHotKey(self.hwnd, HOTKEY_QUIT);
            DeleteObject(self.big_font as HGDIOBJ);
            DeleteObject(self.small_font as HGDIOBJ);
            DeleteObject(self.bg_brush as HGDIOBJ);
            DeleteObject(self.border_pen as HGDIOBJ);
            self.big_font = ptr::null_mut();
            self.small_font = ptr::null_mut();
            self.bg_brush = ptr::null_mut();
            self.border_pen = ptr::null_mut();
            if !self.hwnd.is_null() {
                DestroyWindow(self.hwnd);
                self.hwnd = ptr::null_mut();
            }
        }
    }

    fn position(&self) {
        unsafe {
            let mut work = RECT::default();
            SystemParametersInfoW(SPI_GETWORKAREA, 0, (&mut work as *mut RECT).cast(), 0);
            SetWindowPos(
                self.hwnd,
                ptr::null_mut(),
                work.left + WINDOW_X,
                work.top + self.top,
                WINDOW_W,
                WINDOW_H,
                SWP_NOACTIVATE
                    | if self.visible { SWP_SHOWWINDOW } else { SWP_HIDEWINDOW },
            );
        }
    }

    fn toggle(&mut self) {
        self.visible = !self.visible;
        unsafe {
            ShowWindow(self.hwnd, if self.visible { SW_SHOWNOACTIVATE } else { SW_HIDE });
        }
    }

    fn paint(&mut self, hwnd: HWND) {
        unsafe {
            let mut ps: PAINTSTRUCT = std::mem::zeroed();
            let dc = BeginPaint(hwnd, &mut ps);
            let mut rect = RECT::default();
            GetClientRect(hwnd, &mut rect);
            FillRect(dc, &rect, self.bg_brush);
            SetBkMode(dc, TRANSPARENT_BK);

            let green = 0x82EB_70u32; // RGB(112,235,130)
            let gray = 0x9B91_8Cu32; // RGB(140,145,155)
            let white = 0xF8F3_F0u32;

            let mut big_rect = RECT { left: 14, top: 6, right: rect.right - 10, bottom: 62 };
            if let (Some(d), Some(_p)) = (self.state.distance, self.state.pitch) {
                let mut text = format!("{:.1} m", d);
                let buf = text.encode_utf16().collect::<Vec<u16>>();
                SetTextColor(dc, green);
                SelectObject(dc, self.big_font as HGDIOBJ);
                DrawTextW(dc, buf.as_ptr(), buf.len() as i32, &mut big_rect,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);
            } else {
                let buf: Vec<u16> = "-- m".encode_utf16().collect();
                SetTextColor(dc, gray);
                SelectObject(dc, self.big_font as HGDIOBJ);
                DrawTextW(dc, buf.as_ptr(), buf.len() as i32, &mut big_rect,
                          DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);
            }

            let mut small_rect = RECT { left: 14, top: 64, right: rect.right - 10, bottom: 92 };
            let small: String = match self.state.pitch {
                Some(p) => format!("pitch {:.1}°  置信 {:.2}", p, self.state.confidence),
                None => "未检测到 pitch（--）".to_string(),
            };
            let buf: Vec<u16> = small.encode_utf16().collect();
            SetTextColor(dc, if self.state.pitch.is_some() { white } else { gray });
            SelectObject(dc, self.small_font as HGDIOBJ);
            DrawTextW(dc, buf.as_ptr(), buf.len() as i32, &mut small_rect,
                      DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOCLIP);

            let old_pen = SelectObject(dc, self.border_pen as HGDIOBJ);
            let old_brush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, 1, 1, rect.right - 1, rect.bottom - 1);
            SelectObject(dc, old_brush);
            SelectObject(dc, old_pen);
            EndPaint(hwnd, &ps);
        }
    }
}

impl Drop for Window {
    fn drop(&mut self) {
        self.destroy();
    }
}

fn widestring(s: &str) -> Vec<u16> {
    s.encode_utf16().chain(std::iter::once(0)).collect()
}

unsafe extern "system" fn window_proc(
    hwnd: HWND,
    msg: u32,
    wparam: usize,
    lparam: isize,
) -> isize {
    let win_ptr = GetWindowLongPtrW(hwnd, GWLP_USERDATA);
    if win_ptr == 0 {
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
    let win = &mut *(win_ptr as *mut Window);
    match msg {
        WM_PAINT => {
            win.paint(hwnd);
            0
        }
        WM_ERASEBKGND => 1,
        WM_NCHITTEST => HTTRANSPARENT,
        WM_HOTKEY => {
            if wparam as i32 == HOTKEY_TOGGLE {
                win.toggle();
            } else if wparam as i32 == HOTKEY_QUIT {
                PostQuitMessage(0);
            }
            0
        }
        WM_DISPLAYCHANGE => {
            win.position();
            0
        }
        WM_DESTROY => {
            PostQuitMessage(0);
            0
        }
        _ => DefWindowProcW(hwnd, msg, wparam, lparam),
    }
}
