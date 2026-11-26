-- Private CI data, deliberately bypasses the not-yet-implemented writes.
INSERT INTO players(id,display_name,normalized_name,active) VALUES
 ('00000000-0000-0000-0000-000000000001','Alpha','alpha',true),
 ('00000000-0000-0000-0000-000000000002','Beta','beta',true),
 ('00000000-0000-0000-0000-000000000003','Disabled','disabled',false);
INSERT INTO song_sets(id,owner_id,authoring_id,title,artist)
 VALUES ('10000000-0000-0000-0000-000000000001','00000000-0000-0000-0000-000000000001','fixture','Song','Artist');
INSERT INTO difficulties(id,set_id,authoring_id) VALUES
 ('20000000-0000-0000-0000-000000000001','10000000-0000-0000-0000-000000000001',repeat('a',32)),
 ('20000000-0000-0000-0000-000000000002','10000000-0000-0000-0000-000000000001',repeat('b',32)),
 ('20000000-0000-0000-0000-000000000003','10000000-0000-0000-0000-000000000001',repeat('c',32));
INSERT INTO revisions(id,difficulty_id,title,gameplay_sha256,manifest_sha256,chart_sha256,audio_sha256,
    package_key,audio_duration_ms,difficulty_version,stars,key_presses,publication,ranked,is_current,published_at)
 SELECT ('30000000-0000-0000-0000-00000000000'||n)::uuid,
        ('20000000-0000-0000-0000-00000000000'||n)::uuid,
        'Chart '||n,repeat('a',64),repeat('b',64),repeat('c',64),repeat('d',64),
        'fixture',3000,4,2.0,3,CASE WHEN n=3 THEN 'draft' ELSE 'published' END,
        n=1,n<>3,CASE WHEN n=3 THEN NULL ELSE now() END
 FROM generate_series(1,3) n;
INSERT INTO attempts(id,player_id,revision_id,idempotency_key,rate_percent,mod_flags,mod_seed,judgement_version,replay_key,replay_sha256)
 SELECT ('40000000-0000-0000-0000-00000000000'||n)::uuid,
        ('00000000-0000-0000-0000-00000000000'||CASE WHEN n=3 THEN 2 WHEN n=5 THEN 3 ELSE 1 END)::uuid,
        '30000000-0000-0000-0000-000000000001',
        ('50000000-0000-0000-0000-00000000000'||n)::uuid,100,0,0,1,'fixture',repeat('e',64)
 FROM generate_series(1,6) n;
INSERT INTO verified_scores(attempt_id,perfect,good,bad,miss,max_combo,accuracy_percent,difficulty_version,performance_version,pp,rank_approved)
 SELECT ('40000000-0000-0000-0000-00000000000'||n)::uuid,
        3,0,0,0,3,100,4,CASE WHEN n=6 THEN 2 ELSE 1 END,
        CASE n WHEN 1 THEN 10 WHEN 2 THEN 20 WHEN 3 THEN 15 ELSE 999 END,n<>4
 FROM generate_series(1,6) n;
