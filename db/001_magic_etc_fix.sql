-- 001_magic_etc_fix.sql
-- Purpose: set Etc = 1 rows in the target table to 0 so skills and potions
-- no longer require quest 1 to be completed (ADR-0003, KI-001).
-- Rows with Etc in 510..523 (level 70+ master skills) are never touched;
-- only Etc = 1 rows are updated.
--
-- Usage:
--   sqlcmd -S .\SQLEXPRESS -E -d FDP_kn_online -b -v Target=MAGIC -i db/001_magic_etc_fix.sql
--
-- The Target variable is required on purpose (no default); it names the table
-- to fix. The original Etc value of every changed row is saved in
-- dbo.<Target>_ETC_FIX_BACKUP so 001_magic_etc_fix_rollback.sql can undo this.
-- The script is idempotent: running it again changes nothing.

SET NOCOUNT ON;
SET XACT_ABORT ON;
GO

IF OBJECT_ID(N'dbo.$(Target)_ETC_FIX_BACKUP', N'U') IS NULL
CREATE TABLE dbo.$(Target)_ETC_FIX_BACKUP
(
    MagicNum int NOT NULL PRIMARY KEY,
    OldEtc smallint NOT NULL,
    FixedAt datetime NOT NULL DEFAULT GETDATE()
);
GO

BEGIN TRANSACTION;

-- Save the original Etc value of the rows that are about to change, once.
INSERT INTO dbo.$(Target)_ETC_FIX_BACKUP (MagicNum, OldEtc)
SELECT m.MagicNum, m.Etc
FROM dbo.$(Target) AS m
WHERE m.Etc = 1
  AND NOT EXISTS
  (
      SELECT 1
      FROM dbo.$(Target)_ETC_FIX_BACKUP AS b
      WHERE b.MagicNum = m.MagicNum
  );

-- Apply the fix.
UPDATE dbo.$(Target)
SET Etc = 0
WHERE Etc = 1;

COMMIT TRANSACTION;
GO

-- Verification: one result row.
SELECT
    (SELECT COUNT(*) FROM dbo.$(Target) WHERE Etc = 1) AS etc1_remaining,
    (SELECT COUNT(*) FROM dbo.$(Target) WHERE Etc BETWEEN 510 AND 523) AS etc_510_523,
    (SELECT COUNT(*) FROM dbo.$(Target)_ETC_FIX_BACKUP) AS backup_rows,
    (SELECT COUNT(*) FROM dbo.$(Target)) AS total_rows;
GO
