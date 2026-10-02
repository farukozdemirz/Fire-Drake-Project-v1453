-- 003_bot_quests.sql
-- Purpose: write the class quests of the 12 bot characters (db/002) into
-- USERDATA.strQuest with state 2 (completed), so quest-gated skills
-- (docs/03 MEC-MAG-14, KI-018, MB-15) cast the same as on a human who has
-- finished those quests. ADR-0018 Ek 3, plan F4-27.
-- Only the 12 bot rows are touched (explicit name list); no other row or
-- table is read or written. Row contents are never printed (personal data).
-- The script merges: existing quest records (e.g. the STARTER_SEED_QUEST 500
-- or monster counters) are kept; only the required ids are re-written with
-- state 2. It is idempotent: a second run reports changed=0 once every
-- required id is already present with state 2.
--
-- Record layout of strQuest (DBAgent.cpp:402): 3 bytes per quest, a
-- little-endian uint16 id followed by a uint8 state:
--   byte 3i   = id % 256, byte 3i+1 = id / 256, byte 3i+2 = state.
--
-- Usage (game server MUST be stopped: a logged-in character writes its
-- in-memory quest list back to USERDATA on logout, DBAgent.cpp:930-938):
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v QuestTestPoints=0 -i db/003_bot_quests.sql
--
-- QuestTestPoints is required (no default): 0 leaves strSkill untouched; 1
-- additionally raises the skill points of BotWP_K and BotMF_K so the level 80
-- quest skills (Hell blade 106580, Igzination 110575) can be cast in the
-- runtime check (K8). The old strSkill is backed up and restored by the
-- rollback script. This sqlcmd build aborts a batch that references an
-- undefined variable, so the variable must be passed on every run.
--
-- Undo with db/003_bot_quests_rollback.sql. The backup table
-- dbo.USERDATA_BOT_QUEST_BACKUP is kept by the rollback script.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF '$(QuestTestPoints)' NOT IN ('0', '1')
    RAISERROR(N'QuestTestPoints must be 0 or 1', 16, 1);
GO

IF OBJECT_ID(N'dbo.USERDATA_BOT_QUEST_BACKUP', N'U') IS NULL
CREATE TABLE dbo.USERDATA_BOT_QUEST_BACKUP
(
    strUserID     varchar(21) NOT NULL PRIMARY KEY,
    OldQuestCount smallint NOT NULL,
    OldQuest      binary(600) NOT NULL,
    OldSkill      varbinary(10) NULL,
    SavedAt       datetime NOT NULL DEFAULT GETDATE()
);
GO

BEGIN TRANSACTION;

-- Save the current values of every bot row once (a re-run does not overwrite).
INSERT INTO dbo.USERDATA_BOT_QUEST_BACKUP (strUserID, OldQuestCount, OldQuest, OldSkill)
SELECT u.strUserID, u.sQuestCount, u.strQuest,
       CASE WHEN '$(QuestTestPoints)' = '1'
                 AND u.strUserID IN ('BotWP_K', 'BotMF_K')
            THEN CONVERT(varbinary(10), u.strSkill)
            ELSE NULL
       END
FROM dbo.USERDATA AS u
JOIN (VALUES
    ('BotWP_K'), ('BotWG_K'), ('BotPHD_K'), ('BotPHB_K'), ('BotMF_K'), ('BotMI_K'),
    ('BotWP_E'), ('BotWG_E'), ('BotPHD_E'), ('BotPHB_E'), ('BotMF_E'), ('BotMI_E')
) AS b(charName) ON b.charName = u.strUserID
WHERE NOT EXISTS
(
    SELECT 1 FROM dbo.USERDATA_BOT_QUEST_BACKUP AS x
    WHERE x.strUserID = u.strUserID
);

-- Build the new quest list per bot row.
DECLARE @rows int = 0, @changed int = 0, @keptOther int = 0;
DECLARE @charName varchar(21), @class smallint, @count smallint, @quest binary(600);
DECLARE @newCount int, @newQuest varbinary(600), @i int, @id smallint, @state tinyint;
DECLARE @reqCount int, @metCount int;
DECLARE @required TABLE (id smallint PRIMARY KEY);
DECLARE @met TABLE (id smallint PRIMARY KEY);

DECLARE bot_cursor CURSOR LOCAL FAST_FORWARD FOR
    SELECT u.strUserID, u.[Class], u.sQuestCount, u.strQuest
    FROM dbo.USERDATA AS u
    JOIN (VALUES
        ('BotWP_K'), ('BotWG_K'), ('BotPHD_K'), ('BotPHB_K'), ('BotMF_K'), ('BotMI_K'),
        ('BotWP_E'), ('BotWG_E'), ('BotPHD_E'), ('BotPHB_E'), ('BotMF_E'), ('BotMI_E')
    ) AS b(charName) ON b.charName = u.strUserID
    ORDER BY u.strUserID;

OPEN bot_cursor;
FETCH NEXT FROM bot_cursor INTO @charName, @class, @count, @quest;

WHILE @@FETCH_STATUS = 0
BEGIN
    SET @rows = @rows + 1;
    DELETE FROM @required;

    IF @class IN (106, 206)
        INSERT INTO @required (id) VALUES (51), (510), (511);
    ELSE IF @class IN (110, 210)
        INSERT INTO @required (id) VALUES (53), (515), (516), (517);
    ELSE IF @class IN (112, 212)
        INSERT INTO @required (id) VALUES (54), (518), (519), (520), (521), (522), (523);

    -- No silent truncation: a row already beyond the limit is an error.
    IF @count > 200
        RAISERROR(N'a bot row has more than 200 quest records', 16, 1);

    SET @reqCount = (SELECT COUNT(*) FROM @required);

    -- Idempotency: if every required id is already present with state 2, the
    -- row is left untouched and not counted as changed (ordering does not
    -- count as a change). Otherwise the merge re-writes the row.
    DELETE FROM @met;
    SET @i = 0;
    WHILE @i < @count
    BEGIN
        SET @id = CAST(SUBSTRING(@quest, @i * 3 + 1, 1) AS tinyint)
                + 256 * CAST(SUBSTRING(@quest, @i * 3 + 2, 1) AS tinyint);
        SET @state = CAST(SUBSTRING(@quest, @i * 3 + 3, 1) AS tinyint);

        IF @state = 2 AND EXISTS (SELECT 1 FROM @required AS r WHERE r.id = @id)
            INSERT INTO @met (id)
            SELECT @id WHERE NOT EXISTS (SELECT 1 FROM @met AS m WHERE m.id = @id);

        SET @i = @i + 1;
    END
    SET @metCount = (SELECT COUNT(*) FROM @met);

    IF @metCount <> @reqCount
    BEGIN
        -- Parse the existing list; keep every id that is not in the required set.
        SET @newCount = 0;
        SET @newQuest = CONVERT(varbinary(600), CAST(0x AS varbinary(max)));
        SET @i = 0;
        WHILE @i < @count
        BEGIN
            SET @id = CAST(SUBSTRING(@quest, @i * 3 + 1, 1) AS tinyint)
                    + 256 * CAST(SUBSTRING(@quest, @i * 3 + 2, 1) AS tinyint);
            SET @state = CAST(SUBSTRING(@quest, @i * 3 + 3, 1) AS tinyint);

            IF NOT EXISTS (SELECT 1 FROM @required AS r WHERE r.id = @id)
            BEGIN
                SET @newQuest = CONVERT(varbinary(600),
                      @newQuest
                    + CONVERT(varbinary(1), @id % 256)
                    + CONVERT(varbinary(1), @id / 256)
                    + CONVERT(varbinary(1), @state));
                SET @newCount = @newCount + 1;
                SET @keptOther = @keptOther + 1;
            END
            SET @i = @i + 1;
        END

        -- Append every required id with state 2 (completed), little-endian id.
        DECLARE req_cursor CURSOR LOCAL FAST_FORWARD FOR
            SELECT r.id FROM @required AS r ORDER BY r.id;
        OPEN req_cursor;
        DECLARE @reqId smallint;
        FETCH NEXT FROM req_cursor INTO @reqId;
        WHILE @@FETCH_STATUS = 0
        BEGIN
            SET @newQuest = CONVERT(varbinary(600),
                  @newQuest
                + CONVERT(varbinary(1), @reqId % 256)
                + CONVERT(varbinary(1), @reqId / 256)
                + CONVERT(varbinary(1), 2));
            SET @newCount = @newCount + 1;
            FETCH NEXT FROM req_cursor INTO @reqId;
        END
        CLOSE req_cursor;
        DEALLOCATE req_cursor;

        IF @newCount > 200
            RAISERROR(N'a bot row would exceed the quest limit of 200', 16, 1);

        -- Pad to the fixed 600 byte column.
        SET @newQuest = CONVERT(varbinary(600), @newQuest
            + CONVERT(binary(600), REPLICATE(CAST(0x00 AS varchar(1)), 600)));

        UPDATE dbo.USERDATA
        SET sQuestCount = @newCount, strQuest = @newQuest
        WHERE strUserID = @charName;

        SET @changed = @changed + 1;
    END

    FETCH NEXT FROM bot_cursor INTO @charName, @class, @count, @quest;
END

CLOSE bot_cursor;
DEALLOCATE bot_cursor;

-- Optional skill-point layout for the runtime check: BotWP_K warrior tree 5,
-- BotMF_K mage tree 5, both at 80 (the level 80 quest skills). Total stays 142,
-- tree <= 80, master <= 20 (db/002 invariants). The old strSkill was backed up.
IF '$(QuestTestPoints)' = '1'
BEGIN
    -- Fill the backup OldSkill when the backup row came from an earlier
    -- QuestTestPoints=0 run (NULL then). An existing value is never overwritten,
    -- and this runs before strSkill is changed.
    UPDATE b
    SET b.OldSkill = CONVERT(varbinary(10), u.strSkill)
    FROM dbo.USERDATA_BOT_QUEST_BACKUP AS b
    JOIN dbo.USERDATA AS u ON u.strUserID = b.strUserID
    WHERE b.OldSkill IS NULL AND b.strUserID IN ('BotWP_K', 'BotMF_K');

    UPDATE dbo.USERDATA
    SET strSkill = CONVERT(varchar(10), 0x000000000050002A1400)
    WHERE strUserID = 'BotWP_K';

    UPDATE dbo.USERDATA
    SET strSkill = CONVERT(varchar(10), 0x0000000000502A001400)
    WHERE strUserID = 'BotMF_K';
END

COMMIT TRANSACTION;

-- Result row: changed = rows whose list was re-written; backup = rows saved.
SELECT @changed AS changed, @keptOther AS kept_other,
    (SELECT COUNT(*) FROM dbo.USERDATA_BOT_QUEST_BACKUP) AS backup_rows;
GO

-- Self-check: every required id present with state 2, verified independently of
-- the write/parse rule above (fixed little-endian 3-byte patterns, hand-built),
-- count within the limit, skill points valid when the test layout ran.
DECLARE @rows int = 0, @ok int = 0, @fail int = 0;
DECLARE @charName varchar(21), @class smallint, @count smallint, @quest binary(600), @skill varchar(10);
DECLARE @wantCount int, @found int, @missing int, @skillFail int;
DECLARE @skillText varchar(10);

-- Expected record for each required id: little-endian id + state 2.
--   51 -> 0x330002, 510 -> 0xFE0102, 511 -> 0xFF0102, 53 -> 0x350002,
--   515 -> 0x030202, 516 -> 0x040202, 517 -> 0x050202, 54 -> 0x360002,
--   518..523 -> 0x060202 .. 0x0B0202.
DECLARE @expect TABLE (id smallint PRIMARY KEY, pat binary(3) NOT NULL);
INSERT INTO @expect (id, pat) VALUES
    (51, 0x330002), (510, 0xFE0102), (511, 0xFF0102),
    (53, 0x350002), (515, 0x030202), (516, 0x040202), (517, 0x050202),
    (54, 0x360002),
    (518, 0x060202), (519, 0x070202), (520, 0x080202),
    (521, 0x090202), (522, 0x0A0202), (523, 0x0B0202);

-- Record positions 0..199.
DECLARE @pos TABLE (i int PRIMARY KEY);
DECLARE @j int = 0;
WHILE @j < 200
BEGIN
    INSERT INTO @pos (i) VALUES (@j);
    SET @j = @j + 1;
END

DECLARE @req TABLE (id smallint PRIMARY KEY);

DECLARE chk_cursor CURSOR LOCAL FAST_FORWARD FOR
    SELECT u.strUserID, u.[Class], u.sQuestCount, u.strQuest, u.strSkill
    FROM dbo.USERDATA AS u
    JOIN (VALUES
        ('BotWP_K'), ('BotWG_K'), ('BotPHD_K'), ('BotPHB_K'), ('BotMF_K'), ('BotMI_K'),
        ('BotWP_E'), ('BotWG_E'), ('BotPHD_E'), ('BotPHB_E'), ('BotMF_E'), ('BotMI_E')
    ) AS b(charName) ON b.charName = u.strUserID
    ORDER BY u.strUserID;

OPEN chk_cursor;
FETCH NEXT FROM chk_cursor INTO @charName, @class, @count, @quest, @skill;

WHILE @@FETCH_STATUS = 0
BEGIN
    SET @rows = @rows + 1;

    DELETE FROM @req;
    IF @class IN (106, 206)
        INSERT INTO @req (id) VALUES (51), (510), (511);
    ELSE IF @class IN (110, 210)
        INSERT INTO @req (id) VALUES (53), (515), (516), (517);
    ELSE IF @class IN (112, 212)
        INSERT INTO @req (id) VALUES (54), (518), (519), (520), (521), (522), (523);

    SET @wantCount = (SELECT COUNT(*) FROM @req);

    -- Count required ids whose fixed 3-byte pattern is missing from the
    -- record-aligned positions of the stored list.
    SET @missing = (
        SELECT COUNT(*)
        FROM @req AS r
        JOIN @expect AS e ON e.id = r.id
        WHERE NOT EXISTS
        (
            SELECT 1 FROM @pos AS p
            WHERE p.i < @count AND SUBSTRING(@quest, p.i * 3 + 1, 3) = e.pat
        )
    );
    SET @found = @wantCount - @missing;

    SET @skillFail = 0;
    IF '$(QuestTestPoints)' = '1' AND @charName IN ('BotWP_K', 'BotMF_K')
    BEGIN
        SET @skillText = @skill;
        IF ASCII(SUBSTRING(@skillText, 1, 1)) + ASCII(SUBSTRING(@skillText, 6, 1))
             + ASCII(SUBSTRING(@skillText, 7, 1)) + ASCII(SUBSTRING(@skillText, 8, 1))
             + ASCII(SUBSTRING(@skillText, 9, 1)) <> 142
            OR ASCII(SUBSTRING(@skillText, 6, 1)) > 80
            OR ASCII(SUBSTRING(@skillText, 7, 1)) > 80
            OR ASCII(SUBSTRING(@skillText, 8, 1)) > 80
            OR ASCII(SUBSTRING(@skillText, 9, 1)) > 20
            SET @skillFail = 1;
    END

    IF @count <= 200 AND @found = @wantCount AND @skillFail = 0
        SET @ok = @ok + 1;
    ELSE
        SET @fail = @fail + 1;

    FETCH NEXT FROM chk_cursor INTO @charName, @class, @count, @quest, @skill;
END

CLOSE chk_cursor;
DEALLOCATE chk_cursor;

PRINT 'BOTQUEST: rows=' + CONVERT(varchar(10), @rows)
    + ' ok=' + CONVERT(varchar(10), @ok)
    + ' fail=' + CONVERT(varchar(10), @fail);

IF @fail > 0
    RAISERROR(N'bot quest self-check failed', 16, 1);
GO
