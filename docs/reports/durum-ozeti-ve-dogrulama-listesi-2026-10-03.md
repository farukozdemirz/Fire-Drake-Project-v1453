# Durum özeti ve doğrulama listesi (2026-10-03)

> Okuyan: proje sahibi. Yazan: Claude (2026-10-03 öğleden önce). Bu belge **özettir**; tek kaynak `docs/STATUS.md`, plan ayrıntısı `plans/README.md`, bot kapasitesi `docs/16`. Çelişirse onlar geçerlidir.

## 1. Nerede olduğumuz

| Faz | Durum | Not |
|---|---|---|
| F0 Ortam | Kabul edildi | |
| F1 Veri ve mekanik, F2 Bot oturumu, F3 Telemetri | Geliştirme bitti, **senin kabulünü bekliyor** | Faz raporu taslakları `docs/phase-reports/F1..F3-taslak.md` |
| F4 Aksiyon ve adalet | Büyük ölçüde bitti, döngü ek dilimler yazıyor | F4-01..F4-48 ağırlıklı `KAPANDI` |
| F5 Navigasyon | Algoritmalar bitti, **oyuna bağlı değil** | F5-01..F5-11 `KAPANDI`; sunucuya bağlama F5-55 yapılmadı |
| F6 Sınıf davranışları, F7 Party koordinasyonu | Başlanmadı | |
| F8 Değerlendirme ve 8v8 | Yalnızca analiz araçları var | `tools/bot-outcome-eval.py`, `tools/bot-composition-check.py` |
| F9, F10 | Başlanmadı | |

## 2. Botlar oyunda şu an ne yapıyor

Botlar **komutla oynayan kuklalar**: kendi kararlarını vermiyorlar, verilen komutu doğru ve adil biçimde uyguluyorlar.

**Yapabildikleri** (çoğu senin istemci testlerinde gözlendi):
- Gerçek oyuncu gibi sunucuda oturum açar; ekipman, stat ve HP/MP DB'den gelir.
- Verilen noktaya yürür.
- Melee vurur; skill atar: tek hedefli, alan, uçan, çift tipli, buff, heal, cure, diriltme, summon, warp ve eşya skill'leri dilimleri yapıldı. Cast iptal edilebilir (iptalde hasar çıkmıyor).
- Pot kullanır, oturur/kalkar, hedef seçer.
- Party: davet, kabul/ret, ayrılma, lider devri, atma, party sohbeti.
- Ölünce yeniden doğar.
- Çevreyi tablolar olarak tutar: yakındaki oyuncular, NPC/canavar, hedef HP, görülen skill olayları.
- Zamanlı betik (`/bot script run <ad>`), senaryo ve ölçüm betikleri çalıştırır.
- Adalet koruması: gerçek oyuncunun yapamayacağı bir şeyi yapmalarını engeller (ör. görmediği şeye saldırmak).

**Yapamadıkları:**
- Kendi başına karar vermek: hedef bulmak, savaşa girmek, geri çekilmek, hayatta kalmak (F6).
- Yol bulma oyunda yok: algoritmalar `BotCore` içinde yazılı ve birim testli, ama sunucuya bağlı değil (F5-55).
- Party koordinasyonu, 8v8 maç, öğrenme (F7..F10).
- Quest kilitli skill'ler: planı (F4-27) yazıldı ve doğrulandı; `db/003_bot_quests.sql` betiğinin gerçek DB'ye uygulandığını ve oyunda sınıf skill'lerinin açıldığını **doğrulamadım**.

## 3. Son günlerde olanlar

- Navigasyon paralel hattı tüm konularını bitirdi ve ana hatla birleşti (navigasyon hattı emekli).
- Ana hat F4-27'den F4-49'a ilerledi; her plan DeepSeek uyguladı, Claude ayrıca doğruladı (derleme + test + gerektiğinde çalışma zamanı).
- İşler **`main`'e alındı ve GitHub'a gönderildi** (derleme rc=0, birim testler geçti, force yok). Döngü işleri `gece/2026-10-02` dalında biriktirir; Claude kontrol noktalarında `main`'e alır.
- `CLAUDE.md`: `main`'e commit ve push serbest (2026-10-03 kararın). Otonom döngünün kendi başına push etmesi **etkin değil** (güvenlik sınıflandırıcısı betik değişikliğini reddetti).
- Gözetimsiz denetleyici (`/tmp/claude-1000/supervisor/auto-supervisor.sh`) döngüyü süre sınırı ya da beklenmedik durmada yeniden başlatır; son hedef F10.
- Ön-plan özelliği (DeepSeek uygularken Claude sıradaki bağımsız planı yazar, en fazla 1 plan ileride) `main`'de hazır ve sahte-claude ile sınandı; **çalışan döngü eski betiği kullandığı için henüz etkin değil**, bir sonraki yeniden başlatmada devreye girer.

## 4. Senin kararların

1. **F1, F2, F3 faz kabulleri** (`KABUL_EDILDI` yalnızca sende). Önce bekleyen insan testleri (bölüm 5).
2. **Q-27 (quest kimlikleri):** 51–54 quest kimlikli 32 skill'in şartı sunucuda `UseStanding` sütununda duruyor. Sunucu bu quest'leri uygulamasın mı kalsın, yoksa `Etc`'e taşınıp sunucu da uygulasın mı? Öneri: mevcut hâl. (`docs/18`)
3. **16 mı 20 bot karakteri:** 8v8 için 16 karakter şart (ulus başına 8); 20 = 16 + 4 çeşitlilik karakteri (kompozisyon karşılaştırması için). (`docs/15` §6a)
4. **F4-27 bot quest kurulumu:** botlara sınıfa göre quest verilmesi (savaşçı, büyücü, priest; rogue yok) ve `db/003`'ün uygulanması.
5. **Otonom döngüde Claude'un aldığı kararlar** (başlığında "gözden geçirilmeli" yazanlar): ADR-0002, 0005, 0006, 0007, 0014, 0015, 0016, 0017, 0018, 0031-DEG. ADR-0030..0033-DEG'in teyidi de sende.
6. **Döngünün kendi başına push etmesi:** istersen Claude Code ayarına izin kuralı ekle; yoksa Claude kontrol noktalarında push eder.
7. **Belge hatası (kimlik çakışması):** `docs/18`'de iki ayrı soru `Q-27` numarasını taşıyor (quest kimlikleri ve `ObsTable` tek yönlü görüş). Birinin yeniden numaralanması gerek; döngü o dosyaları yazarken çakışmasın diye sonraya bıraktım.

## 5. Senin yapman gereken testler (istemci gerektirir)

Ayrıntı ve adımlar `docs/STATUS.md` "Proje sahibi testleri (bekleyen)" bölümünde:

- **T-ARCH-05:** GM hesabıyla `+bot` sohbet komutları.
- **T-REGENE-01, T-PARTY-01..03, T-PERC-01:** paket izleyici F4-39 ile genişletildi; artık ölçülebilir (`./tools/build.sh Release --packet-trace` derlemesi).
- **T-CAST-FLY-01 (a, b), T-CAST-CANCEL-01 (b):** uçan skill ve iptal paket ölçümleri.
- **T-MECH-SKILL, T-MECH-BUF-01..08, T-POT-01..03:** skill, buff ve pot davranışları.
- **T-DATA-02, T-DATA-03:** El Morad hesaplarının giriş değerleri ve ekipman kuşanılabilirliği.
- **T-ENV-ARENA-02:** arena koordinatları.
- **Quest değişikliği:** `testing`/`testmage` hesaplarında quest etkisinin oyunda doğrulanması (bu hesapların DB satırlarını okumadım).

## 6. Döngüyü yönetmek

- Anlık durum: `plans/_logs/auto-loop.state`; ayrıntı: `plans/_logs/auto-loop.log`, `plans/_logs/supervisor.log`.
- Durdurmak için: ana klasörde `touch plans/.supervisor-stop` (denetleyici) ve `touch plans/.auto-loop-stop` (döngü, sonraki adım başında).
- Claude kullanım limiti dolarsa döngü sıfırlanmaya kadar kendi bekler.
- Döngü işi `gece/2026-10-02` dalında biriktirir; `main`'e alma gerektiğinde Claude'a söylemen yeter.
