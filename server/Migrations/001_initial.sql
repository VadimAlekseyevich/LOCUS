-- Server IDs are independent of local authoring IDs. Publication, identity
-- mapping and rank approval are operator-only until authenticated writes exist.
CREATE TABLE players (
    id uuid PRIMARY KEY,
    display_name text NOT NULL CHECK (char_length(display_name) BETWEEN 1 AND 64),
    normalized_name text NOT NULL UNIQUE,
    active boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE song_sets (
    id uuid PRIMARY KEY,
    owner_id uuid NOT NULL REFERENCES players(id),
    authoring_id text NOT NULL CHECK (char_length(authoring_id) BETWEEN 1 AND 128),
    title text NOT NULL,
    artist text NOT NULL,
    UNIQUE (owner_id, authoring_id)
);
CREATE TABLE difficulties (
    id uuid PRIMARY KEY,
    set_id uuid NOT NULL REFERENCES song_sets(id),
    authoring_id text NOT NULL CHECK (authoring_id ~ '^[0-9a-f]{32}$'),
    UNIQUE (set_id, authoring_id)
);
CREATE TABLE revisions (
    id uuid PRIMARY KEY,
    difficulty_id uuid NOT NULL REFERENCES difficulties(id),
    title text NOT NULL,
    gameplay_sha256 text NOT NULL CHECK (gameplay_sha256 ~ '^[0-9a-f]{64}$'),
    manifest_sha256 text NOT NULL CHECK (manifest_sha256 ~ '^[0-9a-f]{64}$'),
    chart_sha256 text NOT NULL CHECK (chart_sha256 ~ '^[0-9a-f]{64}$'),
    audio_sha256 text NOT NULL CHECK (audio_sha256 ~ '^[0-9a-f]{64}$'),
    package_key text NOT NULL,
    audio_duration_ms bigint NOT NULL CHECK (audio_duration_ms BETWEEN 1 AND 86400000),
    difficulty_version integer NOT NULL CHECK (difficulty_version > 0),
    stars double precision NOT NULL CHECK (stars >= 0 AND stars < 'Infinity'::float8),
    key_presses integer NOT NULL CHECK (key_presses BETWEEN 1 AND 2000000),
    publication text NOT NULL DEFAULT 'draft' CHECK (publication IN ('draft','published','withdrawn')),
    ranked boolean NOT NULL DEFAULT false,
    is_current boolean NOT NULL DEFAULT false,
    published_at timestamptz,
    CHECK (publication <> 'published' OR published_at IS NOT NULL),
    CHECK (NOT ranked OR publication = 'published'),
    CHECK (NOT is_current OR publication = 'published')
);
CREATE UNIQUE INDEX revisions_one_current ON revisions(difficulty_id) WHERE is_current;
CREATE INDEX revisions_public_catalog ON revisions(id) WHERE is_current AND publication = 'published';

-- A future authenticated submission binds its idempotency key to its owner
-- and trusted session settings, not to values returned by a local client.
CREATE TABLE attempts (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES players(id),
    revision_id uuid NOT NULL REFERENCES revisions(id),
    idempotency_key uuid NOT NULL,
    rate_percent integer NOT NULL CHECK (rate_percent BETWEEN 50 AND 150),
    mod_flags integer NOT NULL CHECK (mod_flags BETWEEN 0 AND 511),
    mod_seed bigint NOT NULL CHECK (mod_seed BETWEEN 0 AND 4294967295),
    judgement_version integer NOT NULL CHECK (judgement_version > 0),
    replay_key text NOT NULL,
    replay_sha256 text NOT NULL CHECK (replay_sha256 ~ '^[0-9a-f]{64}$'),
    received_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE (player_id, idempotency_key)
);
-- Only trusted verifier output belongs here. Rank approval defaults off
-- and is distinct from mathematical verification.
CREATE TABLE verified_scores (
    attempt_id uuid PRIMARY KEY REFERENCES attempts(id),
    perfect integer NOT NULL CHECK (perfect BETWEEN 0 AND 2000000),
    good integer NOT NULL CHECK (good BETWEEN 0 AND 2000000),
    bad integer NOT NULL CHECK (bad BETWEEN 0 AND 2000000),
    miss integer NOT NULL CHECK (miss BETWEEN 0 AND 2000000),
    max_combo integer NOT NULL CHECK (max_combo >= 0),
    accuracy_percent double precision NOT NULL CHECK (accuracy_percent BETWEEN 0 AND 100),
    difficulty_version integer NOT NULL CHECK (difficulty_version > 0),
    performance_version integer NOT NULL CHECK (performance_version > 0),
    pp double precision NOT NULL CHECK (pp >= 0 AND pp < 'Infinity'::float8),
    rank_approved boolean NOT NULL DEFAULT false,
    verified_at timestamptz NOT NULL DEFAULT now(),
    CHECK (perfect + good + bad + miss BETWEEN 1 AND 2000000),
    CHECK (max_combo <= perfect + good + bad)
);
CREATE INDEX attempts_revision_player ON attempts(revision_id, player_id);
