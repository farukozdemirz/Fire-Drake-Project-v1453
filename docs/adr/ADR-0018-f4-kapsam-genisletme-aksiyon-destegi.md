# ADR-0018: F4 kapsam genişletmesi: aksiyon desteğinin tamamlanması (otonom döngüde Claude kararı — gözden geçirilmeli)

Durum: KABUL (geçici, proje sahibi gözden geçirecek) · Tarih: 2026-10-02 · Karar veren: Claude (proje sahibi "eksiksiz ama hızlı" dedi; ana hat F4-23'te "F4 tamamlandı" diyerek durdu)
İlgili: `docs/17` F4 (§2), `docs/phase-reports/F4-taslak.md` §3, ADR-0017 (Ek F4-03: cast sınırı, Ek F4-04: pot, Ek F4-16/18: algı), `docs/05` (skill kataloğu), `docs/03` CLI-03/07/09/12

## Bağlam
F4-01..F4-23 KAPANDI ve `docs/17` F4 listesindeki dilimler bitti, ama faz taslağı (§3) şunları "ertelenen" olarak bırakıyor: uçan, çift tipli (`bType[1] != 0`, buz büyüleri), Type4 (buff/debuff), alan ve eşya tüketen skill'ler `unsupported_skill`; cast iptali ve hareketle iptal yok; CLI-07 (alan hedef noktası) ve CLI-12 (speedhack kontrol paketi) kodda yok; pot/skill tüketimi için envanter doldurma yok; T-MECH-SKILL'in bot tarafından yeniden koşulması buna bağlı ertelenmiş; başkalarının oturma bayrağı izlenmiyor. 2026-10-02 testleri bunu doğruladı: mage'in ana skill'leri (Fire ball, Prismatic) ve priest'in Malice'i botla atılamıyor. Warrior/priest/mage davranışları (F6+) bu aksiyonlar olmadan yazılamaz.

## Karar
Bu işler **F4'ün parçası** sayılır (aksiyon yürütme = "botların temel aksiyonlarını yapabilmesi", `docs/17` F4 amacı). F4, aşağıdaki dilimler bitmeden "tamamlandı" ilan edilmez. Her dilim küçük bir plandır (≤ ~6 dosya, tek yetenek), aynı doğrulama eşiğiyle (derleme + birim test + çalışma zamanı) ilerler. Sıra (plan-olustur her seferinde yalnızca sıradakini yazar; bir dilim gerekirse bölünür):
1. Cast iptali (`MAGIC_FAIL -100`), hareketle iptal, `UseStanding` otomatik durdurma (CLI-03/CLI-09 tam).
2. Uçan skill'ler (`bType[0] == 2` / `FlyingEffect != 0`: Fire ball, Ice arrow, Fire spear...).
3. Çift tipli Type3 skill'ler (`bType[1] != 0`: buz büyüleri, Prismatic).
4. Type4 buff/debuff skill'leri (Malice, AC/HP buff'ları; kendine/dosta/düşmana) ve `BuffView` ile tutarlılık.
5. Alan skill'leri (`bMoral` 10..13, CLI-07 hedef noktası).
6. Cure/diriltme/summon gibi Type5+ ve eşya tüketen (`UseItem`) skill'ler için ilk dilim(ler): hangi tiplerin gerçekten gerektiği `docs/05`/`docs/06..08` ihtiyaçlarına göre seçilir.
7. CLI-12 (`WIZ_SPEEDHACK_CHECK` yanıtı).
8. Pot ve skill tüketimi için envanter doldurma/yeniden stoklama (`ScenarioRunner` ile).
9. T-MECH-SKILL'in botla betikli yeniden koşusu (warrior/priest/mage).
10. Algı eksikleri: başkalarının oturma bayrağı (`WIZ_STATE_CHANGE`), `PARTY_LEVELCHANGE`/`STATUSCHANGE`.

## Sonuçlar
- F4 kabulü bu dilimlerin KAPANDI olmasına ve faz taslağının (§3) güncellenmesine bağlanır.
- Hangi skill tipinin hangi davranış fazında zorunlu olduğu zinciri (davranış → skill → aksiyon desteği → algı → oyun içi kabul) `docs/` değerlendirme çalışmasında ayrıca işlenir; çakışırsa bu ADR'nin sırası kalır, kapsam daraltılabilir ama genişletilmez.
- Faz kabulü (`KABUL_EDILDI`) yine yalnızca proje sahibinindir.

## Ek 1 (2026-10-02, değerlendirme ajanı önerisi; Claude kabulü, proje sahibi gözden geçirecek)
Değerlendirme (`docs/reports/degerlendirme-2026-10-02.md`, §6) ADR'nin belirsiz bıraktığı noktaları şöyle netleştirir; dilimler `plan-olustur` tarafından bu sırayla, her seferinde tek dilim olarak yazılır:
1. **Madde 6 bölünür:** (a) Type5 cure; (b) diriltme (Stone of Life); (c) summon (Type8) ve güvenlik kapıları (hedef yaşıyor mu, güvenli mi); (d) Type8 warp/descent/Gate; (e) eşya tüketen skill'ler (`UseItem`: sınıf taşları, Stone of Warrior/Priest). Hepsi **F7'den (priest/mage davranışı) önce** bitmelidir.
2. **Madde 5 (alan skill'leri):** party hedefli skill'ler (grup heal/buff) için hedef çözümü (kimi etkiler, menzil) ayrı dilimdir.
3. **Type7** (Binding/provoke, Q-22): bu ADR'de karar verilmez; ayrı kısa karar planı (kullanıp kullanmayacağımız ve nasıl ölçüleceği).
4. **Tutarlılık:** madde 2 (uçan) ve madde 5 (alan) mage için `FLYING` fazıyla birlikte ele alınır (uçan alan büyüleri: ör. Fire burst); iki dilim birbirinin kabulünü kırmaz.
5. **Madde 8 (envanter doldurma)** yalnızca pot değil taş/scroll doldurmayı da kapsar ve `ScenarioReset` ile ortak sözleşme olarak tanımlanır (`docs/15` §6a).
6. **Madde 10 (algı eksikleri)** yalnızca oturma/seviye değildir: değerlendirme düzeltme planları `F4-50..54` ile eşlenir (gözlem meta verisi, düşman HP tablosu, skill olay halkası, tek yönlü görüş teşhisi, gözlenen durum).
7. **Kabul:** her dilimin oyun içi kabulü `docs/17` G4 kapısına ve `docs/reports/degerlendirme-takip.md` tablosundaki "oyun içinde doğrulandı" sütununa bağlanır; birim testi veya doküman güncellemesi dilimi "doğrulandı" yapmaz.

## Ek 2 (2026-10-02, F4-25; otonom döngüde Claude kararı — gözden geçirilmeli)
Dilim 2 (uçan skill'ler) ikiye bölünür: **2a** tek hedefli, tek tipli Type3 uçan skill'ler (F4-25: Fire ball, Fire spear, Static orb); **2b** okçu Type2 skill'leri (ok tüketimi ve yay denetimi; ok stoğu için dilim 8 ile birlikte). Gerekçe ve paket düzeni: ADR-0017 Eki F4-25. Dilim sırası değişmez; F4-25'ten sonra sıradaki **F4-26 = dilim 3 (çift tipli Type3)**.

## Ek 3 (2026-10-02, F4-26; otonom döngüde Claude kararı — gözden geçirilmeli)
Dilim 3 (çift tipli Type3) **yalnızca `bType = {3, 4}` çifti** olarak yazıldı (F4-26: buz büyüleri, Prismatic, uçanlar Ice arrow/orb). Diğer çiftler (Type1+Type3/4 melee, Type2 okçu, Type1+Type9, alan, `UseItem`) bu dilimde açılmaz; ilgili dilimlere bırakılır (alan: dilim 5, okçu: 2b, `UseItem`: dilim 6, Type1+Type4 melee çiftleri: warrior davranışı için ayrı küçük dilim gerekirse eklenir). Gerekçe ve sunucu davranışı: ADR-0017 Eki F4-26. Dilim sırası değişmez; F4-26'dan sonra sıradaki **F4-27 = dilim 4 (Type4 tek tipli buff/debuff)**.

## Ek 2 (2026-10-03, proje sahibi test oturumu)
11. **Paket izleyici kapsamı:** `FDP_PACKET_TRACE` (`GameServer/PacketTrace.cpp:24-32`) yalnızca hareket, dönme, saldırı, büyü, hedef HP, durum değişimi ve speedhack kontrol paketlerini kaydeder. İnsan istemcisi zamanlama ölçümleri (CLI-14/15/16/17/18/19/20: yeniden doğ düğmesi, party davet/kabul/ret/ayrılma/devir/atma, sohbet, bölge değişiminde kullanıcı/NPC bilgisi isteği) bu paketlere ihtiyaç duyar; bu yüzden T-PARTY-01..03, T-REGENE-01 ve T-PERC-01 şu an ölçülemez. Küçük bir dilim: `WIZ_PARTY`, `WIZ_REGENE`, `WIZ_REGIONCHANGE`, `WIZ_REQ_USERIN`, `WIZ_NPC_REGION`/`WIZ_REQ_NPCIN`, `WIZ_CHAT` kayıt listesine eklenir (yalnız derleme bayrağıyla, oyun mantığı değişmez) ve `tools/packet-trace-summary.py` `--cli` çıktısına ilgili bölümler ve `--selftest` vakaları eklenir. Sıra: ADR'deki dilimlerden bağımsız, ilk fırsatta.

## Ek 3 (2026-10-03, quest ile açılan skill'ler; plan F4-27)
12. **Quest kilitli skill'ler:** Skill'in quest ile açılması (`docs/03` MEC-MAG-14, KI-018): sunucu `UserCanCast()` Release'de GM olmayanlardan `CheckExistEvent(sEtc, 2)` ister; bot satırlarının quest listesi (`USERDATA.strQuest`) boştur. Karar: (a) bot satırlarına sınıflarının quest'leri **durum 2** olarak yazılır (`db/003_bot_quests.sql`, geri alınabilir; warrior {51, 510, 511}, mage {53, 515, 516, 517}, priest {54, 518–523}; rogue botlarda yok); (b) `BeginCast` `sEtc != 0` skill'i `unsupported_skill` ile değil sunucunun kuralıyla işler: botun quest listesinde durum 2 değilse `quest_locked` (ön kontrol, sunucuya paket gitmez). **Adalet:** bu, ilgili quest'leri tamamlamış bir insan oyuncuyla eşdeğerdir (kural insan ve bot için aynı, K-5, ADR-0017); bot sunucunun yasakladığı hiçbir skill'i atmaz, quest'i tamamlamamış bir bot yine reddedilir. Skill puanı (`KI-016`) ve sınıf denetimleri ayrıdır; referans profillerde ağaçlar ≤ 70 olduğundan 72–80 skill'ler puan yüzünden de erişilemez (profil değişince `db/003` ve puan düzeni birlikte güncellenir). İstemci `.tbl` dosyalarına ve `MAGIC` verisine dokunulmaz; 51–54'ün sunucuda uygulanıp uygulanmayacağı Q-27'dir (öneri: mevcut hâl). Sıra: ADR'deki dilimlerden bağımsız, ilk fırsatta (F4-27).
