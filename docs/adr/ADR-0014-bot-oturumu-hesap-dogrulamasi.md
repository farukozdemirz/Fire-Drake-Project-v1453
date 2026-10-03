# ADR-0014: Bot oturumu hesap doğrulamasını ve `SET_LOGIN_INFO`'yu atlar (otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (gece modu, `AUTO_LOOP=1`; kullanıcıya sorulamadı)
İlgili: K-1 (ADR-0001), K-9 (ADR-0012), F2 (S3, S7), `docs/02` §11 S3/S7, `docs/13` §4.3

## Bağlam
Gerçek bir oyuncu karakter seçmeden önce iki veritabanı adımından geçer: hesap doğrulaması (`WIZ_LOGIN` → `AccountLogin`, parola kontrolü) ve karakter seçiminde `SET_LOGIN_INFO` (`CURRENTUSER` tablosuna satır ekler, `TB_USER.strClientIP` günceller; `GameServer/CharacterSelectionHandler.cpp` `SetLogInInfoToDB`). Botların parolası ve istemci IP'si yoktur (`GetRemoteIP()` soketsiz oturumda `0.0.0.0` döner). `docs/02` S7 iki seçenek sunar: sabit IP veya `SET_LOGIN_INFO`'yu atlamak. `SET_LOGIN_INFO` `bInit=1` ile `CURRENTUSER`'a `INSERT` yapar; bot çıkışı (F2-04) temiz çalışmazsa veya sunucu çökerse artık satır kalır ve sonraki girişte yinelenen kayıt hatası verebilir. `CURRENTUSER` ve `TB_USER` kişisel veri tablolarıdır (`CLAUDE.md` gizlilik kuralı); botların bu tablolara hiç dokunmaması tercih edilir.

## Karar
Bot girişinde:
- Hesap doğrulaması (`AccountLogin`) **yapılmaz**: `BotManager`, bot hesap adını `CUser::m_strAccountID`'ye doğrudan yazar ve `AddAccountName` ile hesap haritasına ekler. Yalnızca `db/002` betiğinin yazdığı 12 sabit bot hesabı/karakteri (`BotManager` içindeki sabit tablo) spawn edilebilir; ini'den gelen rastgele ad kabul edilmez.
- `SET_LOGIN_INFO` **atlanır**: `CUser::SelectCharacter` içinde `m_botSink != nullptr` ise `SetLogInInfoToDB` çağrılmaz. Bot oturumu `CURRENTUSER`/`TB_USER`'a hiç yazmaz.
- Karakter yükleme (`LOAD_USER_DATA`, ambar, premium, kayıtlı büyü) gerçek oyuncuyla aynı kodla çalışır.
- Çıkışta `AccountLogout` **atlanır** (F2-04 eki, 2026-10-02, otonom döngüde Claude kararı; proje sahibi onayladı: 2026-10-03, ADR-0023): `ACCOUNT_LOGOUT` yordamı yalnızca `DELETE FROM CURRENTUSER WHERE strAccountID = @strAccountID` çalıştırır (`OBJECT_DEFINITION` ile okundu, tablo satırı okunmadı). Botların `CURRENTUSER`'da satırı olmadığından çağrı etkisizdir; ama "botlar bu tablolara hiç dokunmaz" ilkesine uymak için `CUser::ReqUserLogOut`'ta (`DatabaseThread.cpp:457`) koşul `m_bLogout != 2 && m_botSink == nullptr` olur. Karakter/ambar/kayıtlı büyü kaydı (`UpdateUser`, `UpdateWarehouseData`, `UpdateSavedMagic`) gerçek oyuncuyla aynı çalışır.

## Değerlendirilen alternatifler
| Alternatif | Artılar | Eksiler | Neden seçilmedi |
|---|---|---|---|
| Sabit IP (`127.0.0.1`) ile `SET_LOGIN_INFO` çalıştır | Gerçek girişe en yakın | `CURRENTUSER`'a artık satır riski; kişisel veri tablolarına yazma; çıkış temizliği F2-04'e bağımlı | Gereksiz risk |
| Bot hesaplarına gerçek parola ile `WIZ_LOGIN` taklidi | Tam giriş yolu | Parola saklama/yönetimi, `TB_USER` bağımlılığı | Botlar için anlamsız |
| Sunucu içi sahte `LogInServer` oturumu | Birebir akış | Büyük ek yüzey | Kapsam dışı |

## Sonuçlar
- Olumlu: `CURRENTUSER`/`TB_USER` kirlenmez; çökme sonrası artık satır yok; giriş yolu kısa.
- Olumsuz: Bot oturumları sunucuda "çevrimiçi hesap" listesi (`CURRENTUSER`) üzerinden görünmez; bu listeyi okuyan harici araçlar (web paneli vb.) botları göstermez. `LoginServer` aynı hesapla gerçek bir giriş denemesine "çevrimiçi değil" der; bot hesapları insan oyuncuya verilmemelidir.
- Risk: `ReqSelectCharacter`'daki yükleme adımları (`LoadPremiumServiceUser` vb.) hesap satırı bekliyorsa spawn başarısız olur; F2-03 çalışma zamanı doğrulamasında görülür.
- Geri alma: `SelectCharacter`'daki tek koşul ve `BotManager::StartSession` kaldırılır.

## Doğrulama
F2-03: `ENABLED=1` ve `SPAWN_ON_START` ile bot `in game` satırı yazılır; kod düzeyinde `SetLogInInfoToDB` çağrısının `m_botSink == nullptr` koşuluna bağlı olduğu `sed -n` ile gösterilir (`CURRENTUSER`/`TB_USER` `CLAUDE.md` gizlilik kuralı gereği sorgulanmaz).
