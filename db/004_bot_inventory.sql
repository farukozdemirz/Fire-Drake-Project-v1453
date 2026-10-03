-- 004_bot_inventory.sql
-- Purpose: refill the consumable inventory slots (14..21) of the 12 bot
-- characters (db/002) with a scenario stock: a single non-consumed 720 HP pot,
-- a single non-consumed 1920 MP pot, the consumed Water of bless (HpPots), the
-- consumed Potion of Ancient Spirit (MpPots), Stone of life (LifeStones), the
-- class stone (ClassStones), the class scroll and (mage only) Spell of impact.
-- The bag is persistent in USERDATA and there is no in-game refill (docs/11
-- STK-04), so T-MECH-SKILL runs and long scripted runs would otherwise start
-- from a depleted bag. ADR-0018 Ek 16, ADR-0032-DEG, plan F4-40.
-- Only the 12 bot rows are touched (explicit name list); the equipment slots
-- (0..13) and the rest of the bag (22..72) are never changed. Row contents are
-- never printed (only counters). Run with the game server stopped: a logged-in
-- character writes its in-memory bag back to USERDATA on logout (ADR-0032-DEG).
--
-- Record layout of strItem: 73 slots x 8 bytes (little-endian int32 item id,
-- int16 durability, int16 count); an empty slot is 8 zero bytes
-- (db/002_bot_characters.sql:218-243, shared/globals.h SLOT_MAX/HAVE_MAX).
-- Slots 14..21 occupy bytes 113..176; this script owns exactly those 64 bytes.
--
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v HpPots=100 -v MpPots=0 -v LifeStones=30 -v ClassStones=50 -i db/004_bot_inventory.sql
--
-- All four variables are required and must be integers 0..9999 (ITEMCOUNT_MAX);
-- this sqlcmd build aborts a batch that references an undefined variable.
-- Defaults matching db/002 are 100 / 0 / 30 / 50; a count of 0 leaves its slot
-- empty. Undo with db/004_bot_inventory_rollback.sql; the backup table
-- dbo.USERDATA_BOT_STOCK_BACKUP is kept by the rollback script.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

-- Required variables: integers in 0..9999.
IF TRY_CAST('$(HpPots)' AS int) IS NULL
    OR TRY_CAST('$(HpPots)' AS int) < 0 OR TRY_CAST('$(HpPots)' AS int) > 9999
    OR TRY_CAST('$(MpPots)' AS int) IS NULL
    OR TRY_CAST('$(MpPots)' AS int) < 0 OR TRY_CAST('$(MpPots)' AS int) > 9999
    OR TRY_CAST('$(LifeStones)' AS int) IS NULL
    OR TRY_CAST('$(LifeStones)' AS int) < 0 OR TRY_CAST('$(LifeStones)' AS int) > 9999
    OR TRY_CAST('$(ClassStones)' AS int) IS NULL
    OR TRY_CAST('$(ClassStones)' AS int) < 0 OR TRY_CAST('$(ClassStones)' AS int) > 9999
    RAISERROR(N'HpPots, MpPots, LifeStones and ClassStones must be integers 0..9999', 16, 1);
GO

-- Schema precheck: strItem must be binary(584), so replacing the 64 owned bytes
-- keeps the row length.
IF NOT EXISTS (
    SELECT 1
    FROM INFORMATION_SCHEMA.COLUMNS
    WHERE TABLE_NAME = 'USERDATA' AND COLUMN_NAME = 'strItem'
      AND DATA_TYPE = 'binary' AND CHARACTER_MAXIMUM_LENGTH = 584
)
    RAISERROR(N'USERDATA.strItem is not binary(584)', 16, 1);

-- Item precheck: every template id must exist in ITEM (db/002:89-97).
DECLARE @missing int;

SELECT TOP (1) @missing = v.itemID
FROM (VALUES
    (389014000),(389015000),(389020000),(389220000),(379006000),
    (379059000),(379061000),(379062000),(379063000),(379065000),
    (379066000),(379070000)
) AS v(itemID)
WHERE NOT EXISTS (SELECT 1 FROM dbo.ITEM i WHERE i.Num = v.itemID)
ORDER BY v.itemID;

IF @missing IS NOT NULL
    RAISERROR(N'ITEM table is missing required item id %d', 16, 1, @missing);
GO

IF OBJECT_ID(N'dbo.USERDATA_BOT_STOCK_BACKUP', N'U') IS NULL
CREATE TABLE dbo.USERDATA_BOT_STOCK_BACKUP
(
    strUserID varchar(21) NOT NULL PRIMARY KEY,
    OldSlots  varbinary(64) NOT NULL,
    SavedAt   datetime NOT NULL DEFAULT GETDATE()
);
GO

DECLARE @hp int = CAST('$(HpPots)' AS int);
DECLARE @mp int = CAST('$(MpPots)' AS int);
DECLARE @life int = CAST('$(LifeStones)' AS int);
DECLARE @cls int = CAST('$(ClassStones)' AS int);

-- Explicit name list (no wildcard match); className picks the class items.
DECLARE @bots TABLE (charName varchar(21) PRIMARY KEY, className varchar(1) NOT NULL);
INSERT INTO @bots (charName, className) VALUES
    ('BotWP_K','W'),('BotWG_K','W'),('BotPHD_K','P'),('BotPHB_K','P'),('BotMF_K','M'),('BotMI_K','M'),
    ('BotWP_E','W'),('BotWG_E','W'),('BotPHD_E','P'),('BotPHB_E','P'),('BotMF_E','M'),('BotMI_E','M');

DECLARE @tmpl TABLE (slot int PRIMARY KEY, itemID int NULL, cnt int NULL);

DECLARE @rows int = 0, @ok int = 0, @fail int = 0, @changed int = 0;
DECLARE @charName varchar(21), @className varchar(1);
DECLARE @strItem binary(584), @strItem2 binary(584), @len int;
DECLARE @block varbinary(64), @cur varbinary(64);
DECLARE @keepBefore varbinary(520), @keepAfter varbinary(520);
DECLARE @slot int, @itemID int, @cnt int, @dur int, @classStone int, @scroll int;

BEGIN TRANSACTION;

-- Save the old 14..21 bytes once; a re-run does not overwrite the backup.
INSERT INTO dbo.USERDATA_BOT_STOCK_BACKUP (strUserID, OldSlots)
SELECT u.strUserID, SUBSTRING(u.strItem, 113, 64)
FROM dbo.USERDATA AS u
JOIN @bots AS b ON b.charName = u.strUserID
WHERE NOT EXISTS
(
    SELECT 1 FROM dbo.USERDATA_BOT_STOCK_BACKUP AS x WHERE x.strUserID = u.strUserID
);

DECLARE bot_cursor CURSOR LOCAL FAST_FORWARD FOR
    SELECT charName, className FROM @bots ORDER BY charName;

OPEN bot_cursor;
FETCH NEXT FROM bot_cursor INTO @charName, @className;

WHILE @@FETCH_STATUS = 0
BEGIN
    SET @rows = @rows + 1;
    SET @strItem = NULL;
    SET @len = NULL;

    SELECT @strItem = u.strItem, @len = DATALENGTH(u.strItem)
    FROM dbo.USERDATA AS u WHERE u.strUserID = @charName;

    -- Precondition: the row exists and keeps the fixed 584 byte layout.
    IF @strItem IS NULL OR @len <> 584
    BEGIN
        SET @fail = @fail + 1;
        FETCH NEXT FROM bot_cursor INTO @charName, @className;
        CONTINUE;
    END

    -- Keep the bytes this script does not own (equipment 0..13, bag 22..72).
    SET @keepBefore = SUBSTRING(@strItem, 1, 112) + SUBSTRING(@strItem, 177, 408);

    -- Class-specific items: warrior / priest / mage.
    IF @className = 'W'
    BEGIN
        SET @classStone = 379059000;
        SET @scroll = 379063000;
    END
    ELSE IF @className = 'P'
    BEGIN
        SET @classStone = 379062000;
        SET @scroll = 379066000;
    END
    ELSE
    BEGIN
        SET @classStone = 379061000;
        SET @scroll = 379065000;
    END

    DELETE FROM @tmpl;
    INSERT INTO @tmpl (slot, itemID, cnt) VALUES
        (14, 389014000, 1),
        (15, CASE WHEN @hp   > 0 THEN 389015000 ELSE NULL END, @hp),
        (16, 389020000, 1),
        (17, CASE WHEN @life > 0 THEN 379006000 ELSE NULL END, @life),
        (18, CASE WHEN @cls  > 0 THEN @classStone ELSE NULL END, @cls),
        (19, @scroll, 1),
        (20, CASE WHEN @className = 'M' THEN 379070000 ELSE NULL END, 1),
        (21, CASE WHEN @mp   > 0 THEN 389220000 ELSE NULL END, @mp);

    -- 64 bytes: 8 slots x (int32 id, int16 durability, int16 count), little-endian.
    SET @block = 0x;
    SET @slot = 14;
    WHILE @slot <= 21
    BEGIN
        SET @itemID = NULL;
        SET @cnt = NULL;
        SELECT @itemID = t.itemID, @cnt = t.cnt FROM @tmpl AS t WHERE t.slot = @slot;

        IF @itemID IS NULL
        BEGIN
            SET @block = @block + 0x0000000000000000;
        END
        ELSE
        BEGIN
            SET @dur = NULL;
            SELECT @dur = i.Duration,
                   @cnt = CASE WHEN i.Countable = 0 THEN 1
                               WHEN @cnt > 9999 THEN 9999
                               ELSE @cnt END
            FROM dbo.ITEM AS i WHERE i.Num = @itemID;

            SET @block = @block
                + CAST(REVERSE(CAST(@itemID AS varbinary(4))) AS varbinary(4))
                + CAST(REVERSE(CAST(@dur AS varbinary(2))) AS varbinary(2))
                + CAST(REVERSE(CAST(@cnt AS varbinary(2))) AS varbinary(2));
        END

        SET @slot = @slot + 1;
    END

    SET @cur = SUBSTRING(@strItem, 113, 64);

    -- Idempotency: a row already at the scenario stock is not written.
    -- STUFF on binary returns varchar in this SQL Server, so the result is
    -- converted back to binary(584); the collation is single-byte, byte-exact.
    IF @block <> @cur
    BEGIN
        UPDATE dbo.USERDATA
        SET strItem = CONVERT(binary(584), STUFF(strItem, 113, 64, @block))
        WHERE strUserID = @charName;
        SET @changed = @changed + 1;
    END

    -- Self-check: owned bytes equal the template, other bytes unchanged, length kept.
    SET @strItem2 = NULL;
    SELECT @strItem2 = u.strItem FROM dbo.USERDATA AS u WHERE u.strUserID = @charName;
    SET @keepAfter = SUBSTRING(@strItem2, 1, 112) + SUBSTRING(@strItem2, 177, 408);

    IF DATALENGTH(@strItem2) = 584
       AND SUBSTRING(@strItem2, 113, 64) = @block
       AND @keepAfter = @keepBefore
        SET @ok = @ok + 1;
    ELSE
        SET @fail = @fail + 1;

    FETCH NEXT FROM bot_cursor INTO @charName, @className;
END

CLOSE bot_cursor;
DEALLOCATE bot_cursor;

-- Single output line: counters only, never row contents.
PRINT 'BOTSTOCK: rows=' + CONVERT(varchar(10), @rows)
    + ' ok=' + CONVERT(varchar(10), @ok)
    + ' fail=' + CONVERT(varchar(10), @fail)
    + ' changed=' + CONVERT(varchar(10), @changed)
    + ' hp=' + CONVERT(varchar(10), @hp)
    + ' mp=' + CONVERT(varchar(10), @mp)
    + ' life=' + CONVERT(varchar(10), @life)
    + ' class=' + CONVERT(varchar(10), @cls);

IF @rows <> 12 OR @fail > 0
BEGIN
    RAISERROR(N'bot stock refill self-check failed', 16, 1);
    ROLLBACK TRANSACTION;
    RETURN;
END

COMMIT TRANSACTION;
GO
