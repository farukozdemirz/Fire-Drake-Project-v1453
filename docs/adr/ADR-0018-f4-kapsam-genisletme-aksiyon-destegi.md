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
