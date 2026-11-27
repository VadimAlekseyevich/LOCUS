-- External authenticated identities are not local player IDs, nicknames or emails.
-- Only a trusted provisioning workflow may insert bindings; no HTTP writes yet.
-- Ordinal comparison is required for both opaque, case-sensitive OIDC claims.
CREATE TABLE player_identities (
    issuer text COLLATE "C" NOT NULL CHECK (octet_length(issuer) BETWEEN 1 AND 512),
    subject text COLLATE "C" NOT NULL CHECK (octet_length(subject) BETWEEN 1 AND 255),
    player_id uuid NOT NULL REFERENCES players(id),
    created_at timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (issuer, subject),
    UNIQUE (player_id, issuer)
);
