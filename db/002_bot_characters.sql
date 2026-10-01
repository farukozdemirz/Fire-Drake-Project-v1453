-- 002_bot_characters.sql
-- Purpose: create the 12 level 80 bot characters (6 role profiles x 2 nations)
-- with master classes, reference S1 gear, potions and a warehouse row, so the
-- F2 bot session can load them (ADR-0002, T-DATA-01).
-- Only the 12 bot accounts/characters are touched (USERDATA, ACCOUNT_CHAR,
-- WAREHOUSE); no personal data table is ever read or written. Run with the
-- game server stopped.
-- Hp/Mp are set high on purpose; the server clamps them to the real maximum
-- at login (User.cpp SetMaxHp/SetMaxMp), so the bots start at full HP/MP.
--
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Upgrade=7 -i db/002_bot_characters.sql
--
-- Upgrade is required and must be 0, 7 or 8; it is the last digit of the
-- armor and weapon item ids. The script is re-runnable: it first removes the
-- 12 bot rows it may have written, then re-creates them. Every written row is
-- checked against the character invariants (level, stats, skills, gear) and
-- the transaction is rolled back on violation.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF '$(Upgrade)' NOT IN ('0', '7', '8')
    RAISERROR(N'Upgrade must be 0, 7 or 8', 16, 1);
GO

-- Ownership precheck: a bot character name may not belong to a non-bot account.
IF EXISTS (
    SELECT 1
    FROM (VALUES
        ('BotWP_K','BotAcc_WP_K'), ('BotWG_K','BotAcc_WG_K'),
        ('BotPHD_K','BotAcc_PHD_K'), ('BotPHB_K','BotAcc_PHB_K'),
        ('BotMF_K','BotAcc_MF_K'), ('BotMI_K','BotAcc_MI_K'),
        ('BotWP_E','BotAcc_WP_E'), ('BotWG_E','BotAcc_WG_E'),
        ('BotPHD_E','BotAcc_PHD_E'), ('BotPHB_E','BotAcc_PHB_E'),
        ('BotMF_E','BotAcc_MF_E'), ('BotMI_E','BotAcc_MI_E')
    ) AS b(charName, account)
    WHERE EXISTS (SELECT 1 FROM dbo.USERDATA u WHERE u.strUserID = b.charName)
      AND NOT EXISTS (
          SELECT 1 FROM dbo.ACCOUNT_CHAR a
          WHERE a.strAccountID = b.account AND a.strCharID1 = b.charName)
)
    RAISERROR(N'a character with a bot name already belongs to a non-bot account', 16, 1);

-- Ownership precheck: a bot account row may not point to a different character.
IF EXISTS (
    SELECT 1
    FROM (VALUES
        ('BotWP_K','BotAcc_WP_K'), ('BotWG_K','BotAcc_WG_K'),
        ('BotPHD_K','BotAcc_PHD_K'), ('BotPHB_K','BotAcc_PHB_K'),
        ('BotMF_K','BotAcc_MF_K'), ('BotMI_K','BotAcc_MI_K'),
        ('BotWP_E','BotAcc_WP_E'), ('BotWG_E','BotAcc_WG_E'),
        ('BotPHD_E','BotAcc_PHD_E'), ('BotPHB_E','BotAcc_PHB_E'),
        ('BotMF_E','BotAcc_MF_E'), ('BotMI_E','BotAcc_MI_E')
    ) AS b(charName, account)
    WHERE EXISTS (SELECT 1 FROM dbo.ACCOUNT_CHAR a WHERE a.strAccountID = b.account)
      AND NOT EXISTS (
          SELECT 1 FROM dbo.ACCOUNT_CHAR a
          WHERE a.strAccountID = b.account AND a.strCharID1 = b.charName)
)
    RAISERROR(N'a bot account already exists and does not list the expected character', 16, 1);
GO

-- Item precheck: every id used below must exist in ITEM (armor and weapons
-- depend on Upgrade), and LEVEL_UP must have the level 80 row.
DECLARE @missing int;
DECLARE @upgrade int = CAST('$(Upgrade)' AS int);

SELECT TOP (1) @missing = v.base + @upgrade
FROM (VALUES
    (206001000),(206002000),(206003000),(206004000),(206005000),
    (286001000),(286002000),(286003000),(286004000),(286005000),
    (266001000),(266002000),(266003000),(266004000),(266005000),
    (156210000),(121310000),(191110000),(181110000)
) AS v(base)
WHERE NOT EXISTS (SELECT 1 FROM dbo.ITEM i WHERE i.Num = v.base + @upgrade)
ORDER BY v.base;

IF @missing IS NOT NULL
    RAISERROR(N'ITEM table is missing required upgradeable item id %d', 16, 1, @missing);

SET @missing = NULL;
SELECT TOP (1) @missing = v.itemID
FROM (VALUES
    (170250256),
    (310310005),(310310007),(320310126),(330110255),(330150257),(330150256),
    (340610107),(340410109),
    (389014000),(389015000),(389020000),(379006000),
    (379059000),(379063000),(379062000),(379066000),
    (379061000),(379065000),(379070000)
) AS v(itemID)
WHERE NOT EXISTS (SELECT 1 FROM dbo.ITEM i WHERE i.Num = v.itemID)
ORDER BY v.itemID;

IF @missing IS NOT NULL
    RAISERROR(N'ITEM table is missing required item id %d', 16, 1, @missing);

IF NOT EXISTS (SELECT 1 FROM dbo.LEVEL_UP WHERE Level = 80)
    RAISERROR(N'LEVEL_UP table has no row for Level = 80', 16, 1);
GO

-- ---------------------------------------------------------------------------
-- Main transaction.
-- ---------------------------------------------------------------------------
DECLARE @bots TABLE
(
    profile  varchar(3),
    charName varchar(21),
    account  varchar(21),
    nation   tinyint,
    race     tinyint,
    class    smallint,
    strong   int,
    sta      int,
    dex      int,
    intel    int,
    cha      int,
    hp       int,
    mp       int,
    skillHex varbinary(10)
);

INSERT INTO @bots (profile, charName, account, nation, race, class, strong, sta, dex, intel, cha, hp, mp, skillHex)
VALUES
    ('WP', 'BotWP_K',  'BotAcc_WP_K',  1,  1, 106, 255, 162,  60,  50,  50, 32000, 32000, 0x00000000004600341400),
    ('WG', 'BotWG_K',  'BotAcc_WG_K',  1,  1, 106, 255, 162,  60,  50,  50, 32000, 32000, 0x00000000003C3E001400),
    ('PHD','BotPHD_K', 'BotAcc_PHD_K', 1,  4, 112, 120, 147,  70, 190,  50, 32000, 32000, 0x00000000003C003E1400),
    ('PHB','BotPHB_K', 'BotAcc_PHB_K', 1,  4, 112, 120, 147,  70, 190,  50, 32000, 32000, 0x00000000003C3E001400),
    ('MF', 'BotMF_K',  'BotAcc_MF_K',  1,  3, 110,  50,  60,  60, 160, 247,  32000, 32000, 0x00000000004634001400),
    ('MI', 'BotMI_K',  'BotAcc_MI_K',  1,  3, 110,  50, 107,  60, 160, 200, 32000, 32000, 0x00000000003446001400),
    ('WP', 'BotWP_E',  'BotAcc_WP_E',  2, 11, 206, 255, 162,  60,  50,  50, 32000, 32000, 0x00000000004600341400),
    ('WG', 'BotWG_E',  'BotAcc_WG_E',  2, 11, 206, 255, 162,  60,  50,  50, 32000, 32000, 0x00000000003C3E001400),
    ('PHD','BotPHD_E', 'BotAcc_PHD_E', 2, 12, 212, 120, 147,  70, 190,  50, 32000, 32000, 0x00000000003C003E1400),
    ('PHB','BotPHB_E', 'BotAcc_PHB_E', 2, 12, 212, 120, 147,  70, 190,  50, 32000, 32000, 0x00000000003C3E001400),
    ('MF', 'BotMF_E',  'BotAcc_MF_E',  2, 12, 210,  50,  60,  60, 160, 247,  32000, 32000, 0x00000000004634001400),
    ('MI', 'BotMI_E',  'BotAcc_MI_E',  2, 12, 210,  50, 107,  60, 160, 200, 32000, 32000, 0x00000000003446001400);

DECLARE @profile varchar(3), @charName varchar(21), @account varchar(21);
DECLARE @nation tinyint, @race tinyint, @class smallint;
DECLARE @strong int, @sta int, @dex int, @intel int, @cha int, @hp int, @mp int;
DECLARE @skillHex varbinary(10), @skillText varchar(10);
DECLARE @strItem varbinary(584);
DECLARE @slots TABLE (slot int PRIMARY KEY, itemID int, cnt int);
DECLARE @slot int, @itemID int, @cnt int, @dur int, @armorPrefix int;
DECLARE @upgrade int = CAST('$(Upgrade)' AS int);

DECLARE bot_cursor CURSOR LOCAL FAST_FORWARD FOR
    SELECT profile, charName, account, nation, race, class, strong, sta, dex, intel, cha, hp, mp, skillHex
    FROM @bots ORDER BY charName;

BEGIN TRANSACTION;

-- Remove any previous run of this script (only the 12 bot rows).
DELETE FROM dbo.WAREHOUSE WHERE strAccountID IN (SELECT account FROM @bots);
DELETE FROM dbo.USERDATA WHERE strUserID IN (SELECT charName FROM @bots);
DELETE FROM dbo.ACCOUNT_CHAR WHERE strAccountID IN (SELECT account FROM @bots);

OPEN bot_cursor;
FETCH NEXT FROM bot_cursor INTO @profile, @charName, @account, @nation, @race, @class, @strong, @sta, @dex, @intel, @cha, @hp, @mp, @skillHex;

WHILE @@FETCH_STATUS = 0
BEGIN
    DELETE FROM @slots;

    -- Armor: 001 BREAST(4), 002 LEG(10), 003 HEAD(1), 004 GLOVE(12), 005 FOOT(13).
    SET @armorPrefix = CASE
        WHEN @class IN (106, 206) THEN 206
        WHEN @class IN (112, 212) THEN 286
        ELSE 266
    END;
    INSERT INTO @slots (slot, itemID, cnt) VALUES
        (4,  @armorPrefix * 1000000 + 1000 + @upgrade, 1),
        (10, @armorPrefix * 1000000 + 2000 + @upgrade, 1),
        (1,  @armorPrefix * 1000000 + 3000 + @upgrade, 1),
        (12, @armorPrefix * 1000000 + 4000 + @upgrade, 1),
        (13, @armorPrefix * 1000000 + 5000 + @upgrade, 1);

    -- Accessories (same at every tier): ears 0/2, neck 3, waist 7, rings 9/11.
    IF @profile IN ('WP', 'WG')
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (0, 310310005, 1), (2, 310310005, 1), (3, 320310126, 1),
            (7, 340610107, 1), (9, 330110255, 1), (11, 330110255, 1);
    ELSE IF @profile IN ('PHD', 'PHB')
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (0, 310310007, 1), (2, 310310007, 1), (3, 320310126, 1),
            (7, 340410109, 1), (9, 330150257, 1), (11, 330150257, 1);
    ELSE
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (0, 310310007, 1), (2, 310310007, 1), (3, 320310126, 1),
            (7, 340410109, 1), (9, 330150256, 1), (11, 330150256, 1);

    -- Right hand (6) and left hand (8).
    IF @profile = 'WP'
        INSERT INTO @slots (slot, itemID, cnt) VALUES (6, 156210000 + @upgrade, 1);
    ELSE IF @profile = 'WG'
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (6, 121310000 + @upgrade, 1), (8, 170250256, 1);
    ELSE IF @profile IN ('PHD', 'PHB')
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (6, 191110000 + @upgrade, 1), (8, 170250256, 1);
    ELSE
        INSERT INTO @slots (slot, itemID, cnt) VALUES (6, 181110000 + @upgrade, 1);

    -- Bag: common consumables from slot 14, then the class-specific extras.
    INSERT INTO @slots (slot, itemID, cnt) VALUES
        (14, 389014000, 1), (15, 389015000, 100), (16, 389020000, 1), (17, 379006000, 30);
    IF @profile IN ('WP', 'WG')
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (18, 379059000, 50), (19, 379063000, 1);
    ELSE IF @profile IN ('PHD', 'PHB')
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (18, 379062000, 50), (19, 379066000, 1);
    ELSE
        INSERT INTO @slots (slot, itemID, cnt) VALUES
            (18, 379061000, 50), (19, 379065000, 1), (20, 379070000, 1);

    -- strItem: 73 slots x 8 bytes (int32 item id, int16 durability, int16 count),
    -- little-endian. Empty slots stay zero.
    SET @strItem = 0x;
    SET @slot = 0;
    WHILE @slot < 73
    BEGIN
        SET @itemID = NULL;
        SET @cnt = NULL;
        SELECT @itemID = s.itemID, @cnt = s.cnt FROM @slots s WHERE s.slot = @slot;

        IF @itemID IS NULL
        BEGIN
            SET @strItem = @strItem + 0x0000000000000000;
        END
        ELSE
        BEGIN
            SET @dur = NULL;
            SELECT @dur = i.Duration,
                   @cnt = CASE WHEN i.Countable = 0 THEN 1
                               WHEN @cnt > 9999 THEN 9999
                               ELSE @cnt END
            FROM dbo.ITEM i WHERE i.Num = @itemID;

            SET @strItem = @strItem
                + CAST(REVERSE(CAST(@itemID AS varbinary(4))) AS varbinary(4))
                + CAST(REVERSE(CAST(@dur AS varbinary(2))) AS varbinary(2))
                + CAST(REVERSE(CAST(@cnt AS varbinary(2))) AS varbinary(2));
        END

        SET @slot = @slot + 1;
    END

    SET @skillText = CAST(@skillHex AS varchar(10));

    INSERT INTO dbo.ACCOUNT_CHAR (strAccountID, bNation, bCharNum, strCharID1, strCharID2, strCharID3)
    VALUES (@account, @nation, 1, @charName, NULL, NULL);

    INSERT INTO dbo.WAREHOUSE (strAccountID, nMoney, dwTime, WarehouseData, strSerial)
    VALUES (@account, 0, 0,
        CONVERT(varbinary(1536), REPLICATE(CAST(0x00 AS varchar(1)), 1536)),
        CONVERT(varbinary(1536), REPLICATE(CAST(0x00 AS varchar(1)), 1536)));

    INSERT INTO dbo.USERDATA
    (
        strUserID, Nation, Race, [Class], HairRGB, Rank, Title, Level, Exp, Loyalty, Face,
        City, Fame, Hp, Mp, Sp, Strong, Sta, Dex, Intel, Cha, Authority, Points,
        Gold, Zone, Bind, PX, PZ, PY, dwTime, strSkill, strItem, strSerial, sQuestCount,
        strQuest, MannerPoint, LoyaltyMonthly, strItemTime
    )
    SELECT
        @charName, @nation, @race, @class, 0, 0, 0, 80,
        (SELECT Exp FROM dbo.LEVEL_UP WHERE Level = 80),
        1000, 1, 0, 0, @hp, @mp, 100, @strong, @sta, @dex, @intel, @cha,
        1, 0, 200000, 71, -1, 127400, 89000, 0, 0, @skillText, @strItem,
        CONVERT(varbinary(584), REPLICATE(CAST(0x00 AS varchar(1)), 584)), 0,
        CONVERT(varbinary(600), REPLICATE(CAST(0x00 AS varchar(1)), 600)), 0, 0,
        CONVERT(varbinary(584), REPLICATE(CAST(0x00 AS varchar(1)), 584));

    FETCH NEXT FROM bot_cursor INTO @profile, @charName, @account, @nation, @race, @class, @strong, @sta, @dex, @intel, @cha, @hp, @mp, @skillHex;
END

CLOSE bot_cursor;
DEALLOCATE bot_cursor;

-- Invariant checks: any violation rolls the whole transaction back.
IF EXISTS (
    SELECT 1
    FROM dbo.USERDATA u
    JOIN @bots b ON b.charName = u.strUserID
    WHERE u.Level <> 80 OR u.Zone <> 71 OR u.Loyalty <= 0 OR u.Authority <> 1 OR u.Points <> 0
       OR u.[Class] NOT IN (106, 110, 112, 206, 210, 212)
       OR NOT ((u.Nation = 1 AND u.Race < 10      AND u.[Class] IN (106, 110, 112))
            OR (u.Nation = 2 AND u.Race > 10      AND u.[Class] IN (206, 210, 212)))
       OR (CAST(u.Strong AS int) + u.Sta + u.Dex + u.Intel + u.Cha) <> 577
       OR u.Strong > 255 OR u.Sta > 255 OR u.Dex > 255 OR u.Intel > 255 OR u.Cha > 255
       OR DATALENGTH(u.strSkill) <> 10
       OR (ASCII(SUBSTRING(u.strSkill, 1, 1)) + ASCII(SUBSTRING(u.strSkill, 2, 1))
         + ASCII(SUBSTRING(u.strSkill, 3, 1)) + ASCII(SUBSTRING(u.strSkill, 4, 1))
         + ASCII(SUBSTRING(u.strSkill, 5, 1)) + ASCII(SUBSTRING(u.strSkill, 6, 1))
         + ASCII(SUBSTRING(u.strSkill, 7, 1)) + ASCII(SUBSTRING(u.strSkill, 8, 1))
         + ASCII(SUBSTRING(u.strSkill, 9, 1)) + ASCII(SUBSTRING(u.strSkill, 10, 1))) <> 142
       OR ASCII(SUBSTRING(u.strSkill, 6, 1)) > 80
       OR ASCII(SUBSTRING(u.strSkill, 7, 1)) > 80
       OR ASCII(SUBSTRING(u.strSkill, 8, 1)) > 80
       OR ASCII(SUBSTRING(u.strSkill, 9, 1)) > 20
       OR DATALENGTH(u.strItem) <> 584
       OR CAST(SUBSTRING(u.strItem, 9, 4) AS int) = 0     -- slot 1  HEAD
       OR CAST(SUBSTRING(u.strItem, 33, 4) AS int) = 0    -- slot 4  BREAST
       OR CAST(SUBSTRING(u.strItem, 49, 4) AS int) = 0    -- slot 6  RIGHTHAND
       OR CAST(SUBSTRING(u.strItem, 81, 4) AS int) = 0    -- slot 10 LEG
       OR CAST(SUBSTRING(u.strItem, 97, 4) AS int) = 0    -- slot 12 GLOVE
       OR CAST(SUBSTRING(u.strItem, 105, 4) AS int) = 0   -- slot 13 FOOT
)
BEGIN
    RAISERROR(N'bot character invariant check failed', 16, 1);
    ROLLBACK TRANSACTION;
    RETURN;
END

IF EXISTS (
    SELECT 1
    FROM @bots b
    WHERE NOT EXISTS (
        SELECT 1 FROM dbo.ACCOUNT_CHAR a
        WHERE a.strAccountID = b.account AND a.bCharNum = 1 AND a.strCharID1 = b.charName)
)
BEGIN
    RAISERROR(N'ACCOUNT_CHAR rows are missing or do not point to the bot characters', 16, 1);
    ROLLBACK TRANSACTION;
    RETURN;
END

COMMIT TRANSACTION;

-- Result: one row per bot character.
SELECT
    b.charName AS [char],
    b.account AS account,
    b.nation AS nation,
    b.race AS race,
    b.class AS [class],
    u.Level AS [level],
    (CAST(u.Strong AS int) + u.Sta + u.Dex + u.Intel + u.Cha) AS stat_sum,
    (ASCII(SUBSTRING(u.strSkill, 1, 1)) + ASCII(SUBSTRING(u.strSkill, 2, 1))
     + ASCII(SUBSTRING(u.strSkill, 3, 1)) + ASCII(SUBSTRING(u.strSkill, 4, 1))
     + ASCII(SUBSTRING(u.strSkill, 5, 1)) + ASCII(SUBSTRING(u.strSkill, 6, 1))
     + ASCII(SUBSTRING(u.strSkill, 7, 1)) + ASCII(SUBSTRING(u.strSkill, 8, 1))
     + ASCII(SUBSTRING(u.strSkill, 9, 1)) + ASCII(SUBSTRING(u.strSkill, 10, 1))) AS skill_sum,
    u.Zone AS zone,
    u.Loyalty AS loyalty,
    c.equipped_items,
    c.bag_items
FROM dbo.USERDATA u
JOIN @bots b ON b.charName = u.strUserID
CROSS APPLY (
    SELECT
        SUM(CASE WHEN d.slot < 14 AND d.id <> 0 THEN 1 ELSE 0 END) AS equipped_items,
        SUM(CASE WHEN d.slot >= 14 AND d.id <> 0 THEN 1 ELSE 0 END) AS bag_items
    FROM (
        SELECT n.slot, CAST(SUBSTRING(u.strItem, n.slot * 8 + 1, 4) AS int) AS id
        FROM (VALUES
            (0),(1),(2),(3),(4),(5),(6),(7),(8),(9),(10),(11),(12),(13),
            (14),(15),(16),(17),(18),(19),(20),(21),(22),(23),(24),(25),(26),(27),
            (28),(29),(30),(31),(32),(33),(34),(35),(36),(37),(38),(39),(40),(41)
        ) AS n(slot)
    ) AS d
) AS c
ORDER BY b.charName;
GO
