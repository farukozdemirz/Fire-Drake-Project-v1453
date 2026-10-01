-- 001_magic_etc_fix_rollback.sql
-- Purpose: undo 001_magic_etc_fix.sql. Restores only rows saved by that script
-- (records kept in dbo.<Target>_ETC_FIX_BACKUP) and only while they are still
-- Etc = 0, so rows changed later by hand are left alone.
--
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=MAGIC -i db/001_magic_etc_fix_rollback.sql
--
-- The Target variable is required on purpose (no default). The backup table
-- itself is not removed, only its restored rows.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF OBJECT_ID(N'dbo.$(Target)_ETC_FIX_BACKUP', N'U') IS NULL
    RAISERROR(N'backup table dbo.$(Target)_ETC_FIX_BACKUP not found', 16, 1);
GO

BEGIN TRANSACTION;

-- Bring back the original Etc value of the rows that are still fixed.
UPDATE m
SET m.Etc = b.OldEtc
FROM dbo.$(Target) AS m
JOIN dbo.$(Target)_ETC_FIX_BACKUP AS b
    ON b.MagicNum = m.MagicNum
WHERE m.Etc = 0;

-- Remove the backup records that were just restored.
DELETE b
FROM dbo.$(Target)_ETC_FIX_BACKUP AS b
JOIN dbo.$(Target) AS m
    ON m.MagicNum = b.MagicNum
WHERE m.Etc = b.OldEtc;

COMMIT TRANSACTION;
GO

-- Verification: restored rows and remaining backup rows (expected 0).
SELECT
    (SELECT COUNT(*) FROM dbo.$(Target) WHERE Etc = 1) AS etc1_after,
    (SELECT COUNT(*) FROM dbo.$(Target)_ETC_FIX_BACKUP) AS backup_rows;
GO
