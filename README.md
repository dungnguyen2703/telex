# telex

Bộ gõ tiếng Việt tối giản cho Windows, chạy dưới khay hệ thống (taskbar).

Ý tưởng: giống UniKey nhưng lược bỏ gần như toàn bộ tùy chọn. Chỉ còn đúng hai
thứ — gõ tiếng Việt kiểu Telex, và một phím tắt để bật/tắt.

## Mục tiêu

- Gõ tiếng Việt kiểu **Telex** trong mọi ứng dụng Windows, hành vi giống UniKey.
- Chạy nền, không có cửa sổ chính. Chỉ hiện một icon ở khay hệ thống.
- Bật/tắt bộ gõ bằng **Alt + Z** ở bất kỳ đâu.
- Không có bảng cài đặt, không có bảng mã để chọn, không có tùy chỉnh phím tắt.

## Phạm vi

### Có

- **Gõ Telex**: các tổ hợp quen thuộc của UniKey.
  - Dấu: `s` sắc, `f` huyền, `r` hỏi, `x` ngã, `j` nặng.
  - Chữ: `aa` → â, `aw` → ă, `ee` → ê, `oo` → ô, `ow` → ơ, `uw` → ư, `dd` → đ.
  - Gõ lại phím dấu để xóa dấu (ví dụ `as` → á, `ass` → as).
  - Bỏ dấu đúng vị trí theo cách UniKey đang làm.
  - Tự hủy dấu khi từ đang gõ không phải là từ tiếng Việt hợp lệ.
- **Bật/tắt**: nhấn `Alt + Z` để bật hoặc tắt bộ gõ. Khi tắt, chương trình
  không can thiệp vào bàn phím, gõ ra ký tự gốc.
- **Danh sách loại trừ**: một file text, mỗi dòng một tên chương trình
  (ví dụ `code.exe`). Khi đang ở trong một ứng dụng có trong danh sách, bộ gõ
  không can thiệp — gõ ra ký tự gốc y như khi đang tắt. Rời khỏi ứng dụng đó thì
  bộ gõ hoạt động lại bình thường.
- **Icon khay hệ thống**: nhìn vào là biết đang bật hay tắt. Nhấp chuột vào icon
  cũng bật/tắt được. Menu chuột phải có hai mục: mở danh sách loại trừ, và thoát.
- **Unicode dựng sẵn**: đầu ra luôn là Unicode, không có lựa chọn nào khác.

### Không có

- Không có bảng mã khác (TCVN3, VNI-Windows, VIQR...).
- Không có kiểu gõ khác (VNI, VIQR).
- Không có cửa sổ cài đặt. File duy nhất là danh sách loại trừ, tự sửa bằng tay.
- Không có bảng gõ tắt, không có macro, không có kiểm tra chính tả.
- Không có chuyển mã clipboard.
- Không có tùy chọn đổi phím tắt — cố định `Alt + Z`.
- Không tự khởi động cùng Windows (người dùng tự thêm nếu muốn).

## Giới hạn đã biết

Không giấu, vì chúng là hệ quả trực tiếp của cách bộ gõ Telex hoạt động —
UniKey cũng vậy:

- **Gõ tiếng Anh bị biến đổi** ở một số từ: `test` → tét, `win` → ưin,
  `password` → pasword. Đó chính là lý do có danh sách loại trừ.
- **`Alt + Z` bị nuốt toàn cục**: ứng dụng bên dưới không nhận được tổ hợp này.
- **Cửa sổ chạy quyền admin không gõ được** nếu telex chạy quyền thường. Đây là
  giới hạn của Windows, muốn dùng thì chạy telex bằng quyền admin.
- **Nguyên âm `uơ`** (thuở, huơ) ra thành `ưở`, vì `uo` + `w` luôn được hiểu là
  `ươ` — vốn phổ biến hơn hàng trăm lần.

## Dùng thử

Cần Visual Studio (bản Community là đủ). Không cần cài thêm gì khác.

```
build.bat        # tạo build\telex.exe
build.bat test   # chạy toàn bộ test
```

Chạy `build\telex.exe`, icon sẽ hiện ở khay hệ thống.

## Nguyên tắc

Đơn giản là mục tiêu chính, không phải là bước đầu của một chương trình lớn hơn.
Mọi tính năng thêm vào đều phải trả lời được câu hỏi: không có nó thì có gõ được
tiếng Việt không? Nếu vẫn gõ được, thì không thêm.

## Ghi chú

Toàn bộ tool này được viết bằng AI — Claude Opus (Anthropic). Con người chỉ mô
tả yêu cầu; phần thiết kế, code và test đều do AI làm.

Tài liệu kỹ thuật nằm trong [docs/](docs/): kiến trúc, đặc tả luật gõ Telex,
kế hoạch test và thứ tự triển khai.
