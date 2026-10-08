# F — 1534 profili duman koşusu (2026-10-08)

> Hazırlayan: Claude · Kapsam: T-UPG-04 (bot regresyonu) çalışma zamanı ön kanıtı · Etiket: `[V]` (kendi koşum)

## Ortam

- Ayrı duman ortamı (canlı kuruluma dokunmadan): çalışma dizini `C:\dev\fdp1534-smoke\server`; DB `FDP_smoke1534` (`.\SQLEXPRESS`, canlı DB'nin 2026-10-08 kopyası + `db/012`, `db/013`, `db/014`); ODBC `KO_SMOKE_GAME` / `KO_SMOKE_MAIN`; portlar GS 15011, LS 15110, AI 10030; `[PROTOCOL] CLIENT_VERSION=1534`.
- Sürücü: `tools/bot-8v8-demo.sh`'nin geçici kopyası (DB adı `FDP_smoke1534`'e çevrildi; depo değişmedi). Varsayılan 6'ya 6 (12 bot), parti kurulumu + beyinler açık.
- Derleme: `yukseltme/1534` Release `--packet-trace`.

## Koşular

| Koşu | Derleme | Botlara giden düzen |
|---|---|---|
| Taban | `yukseltme/1534` + U1-01 (merge `020e4a1d`) | 1453 (profil 1534, ama MyInfo/UserInfo/NpcInfo/REGIONCHANGE henüz dönüştürülmemiş) |
| 1534 | `yukseltme/1534` + U1-01 + U1-02 + U1-03 (merge `3e99edec`) | 1534 (MyInfo 72 eşya, UserInfo +19 bayt, adsız NpcInfo, REGIONCHANGE 0/1/2); BotCore `WireLayout v1534` |

## Sonuç (savaşın ilk 120 sn'si, bot telemetrisi)

| Metrik | Taban | 1534 |
|---|---|---|
| ACTION_SUBMIT | 1.157 | 1.703 |
| HIT_TAKEN | 126 | 173 |
| HEAL | 38 | 69 |
| TARGET_SET | 101 | 129 |
| TEAM_TARGET | 105 | 132 |
| DEBUFF_CAST | 22 | 34 |
| CURE_CAST | 18 | 35 |
| DEATH / RESPAWN | 6 / 6 | 6 / 6 |
| CAST_INTERRUPT | 45 | 30 |
| POTION | 91 | 147 |
| FAIRNESS_REJECT | 0 | 1 (CLI-18 parti sohbeti aralığı; protokolle ilgisiz) |

- Her iki koşuda 12/12 bot oyuna girdi, partiler kuruldu; bot günlüğünde hata yok.
- Paket izi (`PacketTrace_8_10_2026.log`, koşulara bölünerek): taban koşusunda 10 `WIZ_REGIONCHANGE` kaydının hepsi 1453 biçiminde; 1534 koşusunda 27 × (`00`, `01`+liste, `02`) üçlüsü, eski biçim yok.
- Sunucular 121.780 eşya, 603 NPC, 850 canavar, 224 pelerin ile sorunsuz açıldı.

## Hüküm

1534 paket düzenleri botların görme, hedefleme, dövüş, şifa ve diriliş akışını bozmadı; metrikler taban ile aynı büyüklükte (farklar koşudan koşuya olağan oynama). İnsan istemcisiyle giriş testi (T-UPG-01) ayrıca yapılacak.
