# telex

Bộ gõ tiếng Việt tối giản, chạy nền dưới khay hệ thống.

Ý tưởng: giống UniKey nhưng lược bỏ gần như toàn bộ tùy chọn. Chỉ còn đúng hai
thứ — gõ tiếng Việt kiểu Telex, và một phím tắt để bật/tắt.

| Nền tảng | Tình trạng | Phím tắt |
| --- | --- | --- |
| **Windows** | Xong, có bản tải sẵn | `Alt + Z` |
| **macOS** | Xong, có bản tải sẵn | `Control + Space` |

Hai bản được viết **độc lập hoàn toàn**, không dùng chung dòng code nào: bản
Windows viết bằng C++/Win32, bản macOS viết bằng Swift. Thứ dùng chung là tài
liệu trong [docs/](docs/) — luật gõ và bộ test — để hai bản gõ ra kết quả giống
hệt nhau.

## Mục tiêu

- Gõ tiếng Việt kiểu **Telex** trong mọi ứng dụng, hành vi giống UniKey.
- Chạy nền, không có cửa sổ chính. Chỉ hiện một icon nhỏ ở khay hệ thống.
- Bật/tắt bằng một phím tắt ở bất kỳ đâu.
- Không có bảng cài đặt, không có bảng mã để chọn, không có tùy chỉnh phím tắt.

## Phạm vi

### Có

- **Gõ Telex**: các tổ hợp quen thuộc của UniKey.
  - Dấu: `s` sắc, `f` huyền, `r` hỏi, `x` ngã, `j` nặng.
  - Chữ: `aa` → â, `aw` → ă, `ee` → ê, `oo` → ô, `ow` → ơ, `uw` → ư, `dd` → đ.
  - Gõ lại phím dấu để xóa dấu (ví dụ `as` → á, `ass` → as).
  - Bỏ dấu đúng vị trí theo cách UniKey đang làm.
  - Tự hủy dấu khi từ đang gõ không phải là từ tiếng Việt hợp lệ.
- **Bật/tắt**: một phím tắt để bật hoặc tắt bộ gõ (`Alt + Z` trên Windows,
  `Control + Space` trên macOS). Khi tắt, chương trình không can thiệp vào bàn
  phím, gõ ra ký tự gốc.
- **Danh sách loại trừ**: một file text, mỗi dòng một tên chương trình. Khi đang
  ở trong một ứng dụng có trong danh sách, bộ gõ không can thiệp — gõ ra ký tự
  gốc y như khi đang tắt. Rời khỏi ứng dụng đó thì bộ gõ hoạt động lại bình
  thường. Sửa file xong là có hiệu lực ngay, không cần khởi động lại.
- **Icon khay hệ thống**: nhìn vào là biết đang bật hay tắt. Nhấp chuột vào icon
  cũng bật/tắt được. Menu có các mục: mở danh sách loại trừ, khởi động cùng
  Windows (riêng bản Windows, xem bên dưới), và thoát.
- **Unicode dựng sẵn**: đầu ra luôn là Unicode, không có lựa chọn nào khác.

### Không có

- Không có bảng mã khác (TCVN3, VNI-Windows, VIQR...).
- Không có kiểu gõ khác (VNI, VIQR).
- Không có cửa sổ cài đặt. File duy nhất là danh sách loại trừ, tự sửa bằng tay.
- Không có bảng gõ tắt, không có macro, không có kiểm tra chính tả.
- Không có chuyển mã clipboard.
- Không có tùy chọn đổi phím tắt.
- Windows đã có tùy chọn tự khởi động cùng máy trong menu khay hệ thống — xem
  "Riêng Windows" bên dưới. macOS sẽ có sau, hiện chưa làm.

## Giới hạn đã biết

Không giấu, vì chúng là hệ quả trực tiếp của cách bộ gõ Telex hoạt động —
UniKey cũng vậy.

Đúng cho cả hai bản:

- **Gõ tiếng Anh bị biến đổi** ở một số từ: `test` → tét, `win` → ưin,
  `password` → pasword. Đó chính là lý do có danh sách loại trừ.
- **Phím tắt bật/tắt bị nuốt toàn cục**: ứng dụng bên dưới không nhận được tổ
  hợp này.
- **Nguyên âm `uơ`** (thuở, huơ) ra thành `ưở`, vì `uo` + `w` luôn được hiểu là
  `ươ` — vốn phổ biến hơn hàng trăm lần.

Riêng Windows:

- Phím tắt là **`Alt + Z`**.
- **Cửa sổ chạy quyền admin không gõ được** nếu telex chạy quyền thường. Đây là
  giới hạn của Windows, muốn dùng thì chạy telex bằng quyền admin.
- **Khởi động cùng Windows**: chuột phải vào icon khay hệ thống, chọn *Start
  with Windows* để bật/tắt. Chỉ thêm một khoá trong
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, không cài đặt gì thêm.

Riêng macOS:

- Phím tắt là **`Control + Space`**. Vì telex nuốt trọn tổ hợp này, shortcut đổi
  nguồn nhập của hệ thống sẽ không còn tác dụng khi telex đang chạy — telex thay
  thế luôn nó.
- **Phải cấp quyền Accessibility.** Không có quyền này macOS không cho đọc bàn
  phím, app sẽ không làm được gì. Lần đầu chạy telex sẽ nhắc và mở thẳng
  System Settings cho bạn.
- **Ô mật khẩu không gõ được** (Secure Event Input). Cũng vậy với Terminal khi
  bật secure keyboard entry. Đây là giới hạn của macOS, tương đương chuyện cửa
  sổ admin bên Windows.
- App chưa ký số chính thức nên Gatekeeper sẽ chặn lần đầu: chuột phải vào app,
  chọn **Open**, rồi xác nhận.

## Tải về

**Windows** — bản build sẵn nằm ở
[windows/build/telex.exe](windows/build/telex.exe) — bấm vào rồi chọn
**Download**. Không cần cài đặt, chạy thẳng file là xong, icon hiện ở khay hệ
thống.

Windows SmartScreen có thể cảnh báo vì file chưa ký số: chọn **More info** →
**Run anyway**.

**macOS** — bản build sẵn nằm ở [macos/build/telex.app](macos/build/telex.app).
Tải về, kéo vào thư mục Applications rồi chạy. Lần đầu chạy: chuột phải vào app
chọn **Open** để qua Gatekeeper, rồi cấp quyền Accessibility khi được hỏi. Icon
chữ **V** sẽ hiện ở thanh menu.

## Tự build

**Windows.** Cần Visual Studio (bản Community là đủ). Không cần cài thêm gì
khác.

```
cd windows
build.bat        # tạo build\telex.exe
build.bat test   # chạy toàn bộ test
```

Chạy `windows\build\telex.exe`, icon sẽ hiện ở khay hệ thống.

**macOS.** Cần Xcode Command Line Tools. Không cần cài thêm gì khác.

```
cd macos
./build.sh          # tạo build/telex.app
./build.sh engine   # chạy test engine
./build.sh test     # chạy toàn bộ test
```

Lưu ý: mỗi lần build lại thì chữ ký của app đổi, nên phải cấp lại quyền
Accessibility — xoá mục cũ bằng nút `−` rồi thêm lại bằng `+`.

## Nguyên tắc

Đơn giản là mục tiêu chính, không phải là bước đầu của một chương trình lớn hơn.
Mọi tính năng thêm vào đều phải trả lời được câu hỏi: không có nó thì có gõ được
tiếng Việt không? Nếu vẫn gõ được, thì không thêm.

## Ghi chú

Toàn bộ tool này được viết bằng AI — Claude Opus (Anthropic). Con người chỉ mô
tả yêu cầu; phần thiết kế, code và test đều do AI làm.

Tài liệu kỹ thuật:

- [docs/](docs/) — dùng chung cho cả hai bản: luật gõ Telex, kiến trúc bắt buộc,
  và bộ test tier 1 mà cả hai bản phải qua.
- [windows/docs/](windows/docs/) — riêng bản Windows: các bẫy Win32, test e2e,
  thứ tự triển khai.
- [macos/docs/](macos/docs/) — riêng bản macOS: các bẫy của CGEventTap, quyền
  Accessibility, test e2e, thứ tự triển khai.
