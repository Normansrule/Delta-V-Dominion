// Delta-V: Dominion site. No dependencies. Motion is spring-based and respects reduced motion.
(() => {
  "use strict";
  const $ = (s, r = document) => r.querySelector(s);
  const $$ = (s, r = document) => [...r.querySelectorAll(s)];
  const reduceMotion = window.matchMedia("(prefers-reduced-motion: reduce)");
  const store = {
    get(k, d) { try { const v = localStorage.getItem(k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) { /* private window: keep it in memory */ } },
  };

  // A critically damped-ish spring: x'' = -k(x - target) - c x'. Used for the Raven and the squad markers.
  class Spring {
    constructor(value, stiffness = 170, damping = 26) { this.x = value; this.v = 0; this.target = value; this.k = stiffness; this.c = damping; }
    step(dt) {
      const a = -this.k * (this.x - this.target) - this.c * this.v;
      this.v += a * dt; this.x += this.v * dt;
      return Math.abs(this.x - this.target) > 0.0005 || Math.abs(this.v) > 0.0005;
    }
    snap(v) { this.x = this.target = v; this.v = 0; }
  }

  /* ---------------- Theme ---------------- */
  const root = document.documentElement;
  const toggle = $("[data-theme-toggle]");
  const isLight = () => root.dataset.theme ? root.dataset.theme === "light" : window.matchMedia("(prefers-color-scheme: light)").matches;
  const labelTheme = () => toggle && toggle.setAttribute("aria-label", isLight() ? "Switch to dark mode" : "Switch to light mode");
  if (toggle) {
    labelTheme();
    toggle.addEventListener("click", () => {
      root.dataset.theme = isLight() ? "dark" : "light";
      try { localStorage.setItem("dv-theme", root.dataset.theme); } catch (e) {}
      labelTheme();
    });
  }

  /* ---------------- Frames: loading and error states ---------------- */
  $$("[data-frame]").forEach(fig => {
    const img = $("img", fig);
    if (!img) return;
    fig.dataset.alt = img.alt;
    // Listen for the whole life of the image, not just the first load: a lazy image can fail later.
    img.addEventListener("load", () => fig.classList.remove("is-loading", "is-broken"));
    img.addEventListener("error", () => { fig.classList.remove("is-loading"); fig.classList.add("is-broken"); });
    if (img.complete) { if (!img.naturalWidth) fig.classList.add("is-broken"); }
    else fig.classList.add("is-loading");
  });

  /* ---------------- Hero: the Raven lands on pad two as you scroll ---------------- */
  const hero = $(".hero");
  const raven = $("[data-raven]");
  const shadow = $("[data-raven-shadow]");
  const pad = $(".pad-2");
  const status = $("[data-deck-status]");
  const engines = $$("[data-engine]");
  if (hero && raven) {
    const land = new Spring(0, 120, 22);
    let landed = false;
    const render = p => {
      // p: 0 on approach (high, off to the side), 1 down on pad two.
      const e = p;
      const x = 640 + (400 - 640) * e, y = 110 + (650 - 110) * e;
      const s = 1.6 - 0.6 * e, yaw = 32 * (1 - e);
      raven.setAttribute("transform", `translate(${x} ${y}) rotate(${yaw}) scale(${s})`);
      // The shadow sits on the deck below and behind the aircraft and converges as it comes down.
      const off = 90 * (1 - e);
      shadow.setAttribute("transform", `translate(${x + off} ${y + off * 1.2}) rotate(${yaw}) scale(${1 + 0.15 * (1 - e)})`);
      shadow.style.opacity = String(0.35 + 0.65 * e);
      engines.forEach(en => en.style.opacity = String(0.85 - 0.6 * e));
      const now = e > 0.985;
      if (now !== landed) {
        landed = now;
        pad.classList.toggle("is-down", landed);
        status.parentElement.classList.toggle("is-down", landed);
        status.textContent = landed ? "Down and locked on pad two. Welcome aboard." : "Raven Two-One on approach, pad two";
      }
    };
    const progress = () => {
      const r = hero.getBoundingClientRect();
      const span = Math.max(1, hero.offsetHeight - window.innerHeight * 0.9);
      return Math.min(1, Math.max(0, -r.top / span * 1.25));
    };
    let raf = 0, last = 0;
    const tick = t => {
      const dt = Math.min(0.033, (t - last) / 1000 || 0.016); last = t;
      land.target = progress();
      const moving = land.step(dt);
      render(Math.min(1, Math.max(0, land.x)));
      raf = moving ? requestAnimationFrame(tick) : 0;
    };
    const kick = () => { if (!raf) { last = performance.now(); raf = requestAnimationFrame(tick); } };
    if (reduceMotion.matches) { render(1); }
    else { render(0); window.addEventListener("scroll", kick, { passive: true }); kick(); }
    reduceMotion.addEventListener?.("change", () => { if (reduceMotion.matches) { cancelAnimationFrame(raf); raf = 0; render(1); } else kick(); });
  }

  /* ---------------- Progress meters ---------------- */
  const meters = $("[data-meters]");
  if (meters) {
    const data = [["Combat and squad", 80], ["Missions and maps", 45], ["Vehicles", 75], ["Kestrel hub", 75], ["Towns and factions", 35],
      ["Co-op", 25], ["Art and audio", 15], ["Campaign and story", 20]];
    const overall = Math.round(data.reduce((a, d) => a + d[1], 0) / data.length);
    const row = (name, v, cls = "") => `<dt class="${cls}">${name}</dt><dd class="bar-cell"><div class="meter" role="meter" aria-valuemin="0" aria-valuemax="100" aria-valuenow="${v}" aria-label="${name}"><span style="--v:0" data-v="${v / 100}"></span></div></dd><dd class="num ${cls}">${v}%</dd>`;
    meters.innerHTML = row("Overall", overall, "overall") + data.map(d => row(d[0], d[1])).join("");
    const fill = () => $$("span[data-v]", meters).forEach(s => s.style.setProperty("--v", s.dataset.v));
    if (reduceMotion.matches || !("IntersectionObserver" in window)) fill();
    else new IntersectionObserver((es, o) => { if (es.some(e => e.isIntersecting)) { fill(); o.disconnect(); } }, { threshold: 0.3 }).observe(meters);
  }

  /* ---------------- Roll call wall ---------------- */
  const wall = $("[data-wall]");
  const wallEmpty = $("[data-wall-empty]");
  const wallClear = $("[data-wall-clear]");
  const fallen = store.get("dv-wall", []);
  const plate = (s, cls, isNew) => {
    const li = document.createElement("li");
    li.className = `plate ${cls}${isNew ? " is-new" : ""}`;
    li.innerHTML = `<span class="who"></span><span class="what"></span>`;
    li.firstChild.textContent = s.name;
    li.lastChild.textContent = s.what;
    return li;
  };
  const drawWall = (fresh = []) => {
    wall.replaceChildren(...fallen.map(s => plate(s, "kia", fresh.includes(s))));
    wallEmpty.hidden = fallen.length > 0;
    wallClear.hidden = fallen.length === 0;
  };
  if (wall) {
    drawWall();
    wallClear.addEventListener("click", () => { fallen.length = 0; store.set("dv-wall", fallen); drawWall(); wallEmpty.focus?.(); });
  }

  /* ---------------- Plan an operation ---------------- */
  const map = $("[data-map]");
  if (!map) return;
  const NS = "http://www.w3.org/2000/svg";
  const el = (tag, attrs = {}, parent) => { const n = document.createElementNS(NS, tag); for (const k in attrs) n.setAttribute(k, attrs[k]); parent && parent.appendChild(n); return n; };
  const W = 1000, H = 620;
  const rocks = [ // cover on Tharsis Ridge
    { x: 330, y: 400, r: 46 }, { x: 420, y: 250, r: 38 }, { x: 560, y: 470, r: 52 }, { x: 600, y: 160, r: 40 },
    { x: 250, y: 220, r: 34 }, { x: 690, y: 380, r: 36 }, { x: 470, y: 560, r: 30 },
  ];
  const gRocks = $("[data-rocks]", map), gEnemies = $("[data-enemies]", map), gSquad = $("[data-squad]", map), gArcs = $("[data-arcs]", map);
  rocks.forEach(r => el("circle", { cx: r.x, cy: r.y, r: r.r, class: "m-rock" }, gRocks));

  const ROSTER = ["Freya Yilmaz", "Rafael Fujita", "Nadia Okafor", "Kenji Tremblay", "Bao Mbeki", "Oskar Haddad", "Ingrid Ivanova", "Tariq Novak",
    "Amara Castellanos", "Leila Sato", "Yusuf Zhou", "Ines Rourke", "Dmitri Jaramillo", "Ximena Whitlock", "Tomas Reyes", "Sana Kowalski"];
  const RANKS = ["Pvt.", "Pvt.", "PFC", "PFC", "Cpl."];
  const pick = () => { const pool = ROSTER.filter(n => !fallen.some(f => f.name.endsWith(n))); const n = pool.splice(Math.floor(Math.random() * pool.length), 1)[0] || "Replacement"; return `${RANKS[Math.floor(Math.random() * RANKS.length)]} ${n}`; };

  let squad, enemies, order = "move", busy = false;
  const reticle = $("[data-reticle]", map);
  const hint = $("[data-hint]");
  const log = $("[data-log]");
  const runBtn = $("[data-run]"), resetBtn = $("[data-reset]");
  const progressBar = $("[data-progress]");
  const aar = $("[data-aar]");
  const ret = { x: 500, y: 310 };

  const setHint = (text, error = false) => { hint.textContent = text; hint.classList.toggle("is-error", error); };
  const say = (who, text, cinder = false) => {
    const empty = $(".log-empty", log); if (empty) empty.remove();
    const li = document.createElement("li");
    const b = document.createElement("b"); b.textContent = who; if (cinder) b.className = "cinder";
    li.append(b, document.createTextNode(text));
    log.appendChild(li); log.scrollTop = log.scrollHeight;
  };

  const setup = () => {
    gSquad.replaceChildren(); gEnemies.replaceChildren(); gArcs.replaceChildren();
    squad = [{ name: "You", you: true }, { name: pick() }, { name: pick() }, { name: pick() }].map((m, i) => {
      const home = { x: 110 + (i % 2) * 44 - 22, y: 520 + Math.floor(i / 2) * 44 - 22 };
      const g = el("g", { class: `m-mate${m.you ? " is-you" : ""}` }, gSquad);
      el("circle", { r: 20 }, g);
      const t = el("text", {}, g); t.textContent = initial(m);
      return { ...m, g, sx: new Spring(home.x), sy: new Spring(home.y), cover: false, suppressing: null, flanking: false, damage: 0 };
    });
    enemies = [{ x: 760, y: 220 }, { x: 820, y: 360 }, { x: 740, y: 330 }, { x: 860, y: 250 }, { x: 700, y: 260 }].map(p => {
      const g = el("g", { class: "m-enemy" }, gEnemies);
      el("path", { d: "M0 0 L-120 -60 A134 134 0 0 0 -120 60 Z", class: "m-cone" }, g);
      el("rect", { x: -11, y: -11, width: 22, height: 22, transform: "rotate(45)", class: "m-core" }, g);
      g.setAttribute("transform", `translate(${p.x} ${p.y})`);
      return { ...p, g, pinned: false, down: false };
    });
    squad.forEach(drawMate);
    drawRoster();
    log.replaceChildren(); const li = document.createElement("li"); li.className = "log-empty"; li.textContent = "No orders yet. Your squad is waiting at the landing zone."; log.appendChild(li);
    aar.hidden = true; runBtn.disabled = false;
    setHint("Pick an order, then choose a spot on the map.");
  };
  const initial = m => m.you ? "Y" : m.name.split(" ").pop()[0];
  const roster = $("[data-roster]");
  const drawRoster = (results) => {
    roster.replaceChildren(...squad.map(m => {
      const li = document.createElement("li");
      const st = results ? statusOf(m) : "ready";
      if (st === "kia" || st === "wounded") li.className = st;
      const tag = document.createElement("span"); tag.className = `tag${m.you ? " you" : ""}`; tag.textContent = initial(m); tag.setAttribute("aria-hidden", "true");
      const name = document.createElement("span"); name.textContent = m.you ? "You (Commander)" : m.name;
      const s = document.createElement("span"); s.className = "st";
      s.textContent = { ready: "Ready", ok: "Returned", wounded: "Medbay", kia: "Killed in action" }[st];
      li.append(tag, name, s);
      return li;
    }));
  };
  const drawMate = m => m.g.setAttribute("transform", `translate(${m.sx.x.toFixed(1)} ${m.sy.x.toFixed(1)})`);

  // Move every squad marker toward its spring target; springs give the squad weight without slowing orders down.
  let raf = 0, last = 0;
  const tick = t => {
    const dt = Math.min(0.033, (t - last) / 1000 || 0.016); last = t;
    let moving = false;
    squad.forEach(m => { moving = m.sx.step(dt) | m.sy.step(dt) || moving; drawMate(m); });
    raf = moving ? requestAnimationFrame(tick) : 0;
  };
  const animate = () => {
    if (reduceMotion.matches) { squad.forEach(m => { m.sx.snap(m.sx.target); m.sy.snap(m.sy.target); drawMate(m); }); return; }
    if (!raf) { last = performance.now(); raf = requestAnimationFrame(tick); }
  };

  const inRock = p => rocks.find(r => Math.hypot(p.x - r.x, p.y - r.y) < r.r - 4);
  const nearRock = p => rocks.find(r => Math.hypot(p.x - r.x, p.y - r.y) < r.r + 50);

  const ORDERS = {
    move(p) {
      if (inRock(p)) return "Can't move there: that's solid rock. Pick open ground next to it for cover.";
      if (p.x > 690) return "Too close to the relay. The frames would cut you down before you set up. Stop short of it.";
      squad.forEach((m, i) => { const a = (i / squad.length) * Math.PI * 2; m.sx.target = p.x + Math.cos(a) * 34; m.sy.target = p.y + Math.sin(a) * 34; m.suppressing = null; m.flanking = false; });
      say("Halloran", nearRock(p) ? "Moving to that rock. Good cover." : "Moving. Nothing to hide behind out there, Commander.");
    },
    suppress(p) {
      const target = enemies.filter(e => !e.down).sort((a, b) => Math.hypot(a.x - p.x, a.y - p.y) - Math.hypot(b.x - p.x, b.y - p.y))[0];
      if (!target || Math.hypot(target.x - p.x, target.y - p.y) > 120) return "Nothing there to suppress. Pick a spot close to a frame.";
      enemies.forEach(e => { if (Math.hypot(e.x - target.x, e.y - target.y) < 110) { e.pinned = true; e.g.classList.add("is-pinned"); } });
      squad.slice(1).forEach(m => m.suppressing = target);
      say("Halloran", "Laying down fire! Keep their heads down!");
    },
    flank(p) {
      if (inRock(p)) return "Can't flank into rock. Pick open ground beside the target.";
      const pair = squad.slice(2);
      gArcs.replaceChildren();
      pair.forEach((m, i) => {
        const tx = p.x + (i ? 30 : -30), ty = p.y + (i ? 26 : -26);
        el("path", { d: `M${m.sx.x} ${m.sy.x} Q ${(m.sx.x + tx) / 2} ${Math.min(m.sy.x, ty) - 160} ${tx} ${ty}`, class: "m-arc" }, gArcs);
        m.sx.target = tx; m.sy.target = ty; m.flanking = true;
      });
      squad[1].suppressing = squad[1].suppressing || enemies.find(e => !e.down) || null;
      say("Halloran", "Two of ours swinging wide. One stays to keep them busy.");
    },
    hold() { squad.forEach(m => { m.sx.target = m.sx.x; m.sy.target = m.sy.x; }); say("Halloran", "Holding here."); },
    regroup() {
      const you = squad[0];
      squad.slice(1).forEach((m, i) => { m.sx.target = you.sx.target + (i - 1) * 34; m.sy.target = you.sy.target + 34; m.suppressing = null; m.flanking = false; });
      gArcs.replaceChildren(); say("Halloran", "On you, Commander.");
    },
  };

  const issue = p => {
    if (busy) return;
    const err = ORDERS[order](p);
    if (err) { setHint(err, true); return; }
    setHint(order === "hold" || order === "regroup" ? "Order given. Pick another, or run the op." : "Order given. Add another, or run the op.");
    animate();
  };

  const selectOrder = o => {
    order = o;
    $$("[data-order]").forEach(b => b.setAttribute("aria-pressed", String(b.dataset.order === o)));
    if (o === "hold" || o === "regroup") issue(ret);
    else setHint(o === "move" ? "Move: choose open ground. Next to a rock is cover." : o === "suppress" ? "Suppress: choose a frame to pin down." : "Flank: choose a spot beside the frames.");
  };
  $$("[data-order]").forEach(b => b.addEventListener("click", () => selectOrder(b.dataset.order)));

  const toMap = e => {
    const pt = map.createSVGPoint(); pt.x = e.clientX; pt.y = e.clientY;
    const p = pt.matrixTransform(map.getScreenCTM().inverse());
    return { x: Math.max(0, Math.min(W, p.x)), y: Math.max(0, Math.min(H, p.y)) };
  };
  const placeReticle = () => reticle.setAttribute("transform", `translate(${ret.x} ${ret.y})`);
  map.addEventListener("pointermove", e => { if (e.pointerType === "mouse") { Object.assign(ret, toMap(e)); placeReticle(); reticle.classList.add("is-shown"); } });
  map.addEventListener("pointerleave", () => reticle.classList.remove("is-shown"));
  map.addEventListener("click", e => { Object.assign(ret, toMap(e)); placeReticle(); if (order !== "hold" && order !== "regroup") issue({ ...ret }); });
  map.addEventListener("keydown", e => {
    const step = e.shiftKey ? 60 : 20;
    const moves = { ArrowLeft: [-step, 0], ArrowRight: [step, 0], ArrowUp: [0, -step], ArrowDown: [0, step] };
    if (moves[e.key]) { e.preventDefault(); ret.x = Math.max(0, Math.min(W, ret.x + moves[e.key][0])); ret.y = Math.max(0, Math.min(H, ret.y + moves[e.key][1])); placeReticle(); }
    else if (e.key === "Enter" || e.key === " ") { e.preventDefault(); if (order !== "hold" && order !== "regroup") issue({ ...ret }); }
  });
  // The game's own keys pick orders while the map has focus.
  document.addEventListener("keydown", e => {
    if (e.target.closest && e.target.closest("input, textarea")) return;
    if (!map.contains(document.activeElement) && document.activeElement !== map) return;
    const k = { f: "move", t: "suppress", x: "flank", g: "hold", v: "regroup" }[e.key.toLowerCase()];
    if (k) { e.preventDefault(); selectOrder(k); }
  });

  // Resolve the op with the same ideas the game uses: exposure to unpinned frames, cover, flanking, and 50% damage = medbay.
  const resolve = () => {
    const live = enemies.filter(e => !e.down);
    const flankers = squad.filter(m => m.flanking).length;
    const pinned = live.filter(e => e.pinned).length;
    const reasons = [];
    squad.forEach(m => {
      const p = { x: m.sx.target, y: m.sy.target };
      m.cover = !!nearRock(p);
      const threats = live.filter(e => !e.pinned && Math.hypot(e.x - p.x, e.y - p.y) < 520);
      let dmg = threats.length * (m.cover ? 9 : 24) * (0.7 + Math.random() * 0.6);
      if (m.flanking) dmg *= pinned ? 0.6 : 1.3;   // flanking works when someone keeps their heads down
      m.damage = Math.min(100, dmg);
      if (!m.cover && threats.length) reasons.push(`${m.you ? "You were" : m.name.split(" ").pop() + " was"} in the open with ${threats.length} frame${threats.length > 1 ? "s" : ""} free to shoot`);
    });
    const punch = 1 + flankers * 0.5 + pinned * 0.3 + squad.filter(m => m.cover).length * 0.2;
    live.forEach(e => { if (Math.random() < Math.min(0.95, 0.22 * punch)) e.down = true; });
    const win = enemies.filter(e => !e.down).length <= 1;
    return { win, reasons };
  };

  const statusOf = m => m.damage >= 92 ? "kia" : m.damage >= 50 ? "wounded" : "ok";
  const run = async () => {
    if (busy) return;
    if ($(".log-empty", log)) { setHint("Give your squad at least one order first. Move them up to cover, or suppress the frames.", true); return; }
    busy = true; runBtn.disabled = true; resetBtn.disabled = true; aar.hidden = true;
    setHint("Running the op…");
    const { win, reasons } = resolve();
    const lines = [["Halloran", "Contact! Frames by the relay!"], ["CINDER", "Accord unit detected. Your surrender would be efficient.", true],
      ["Halloran", win ? "They're breaking! Push!" : "We're taking fire from everywhere!"], ["Vance (Kestrel)", win ? "Relay's dark. Good work, Delta." : "Pull back to the LZ, Delta. That's an order."]];
    progressBar.hidden = false;
    const bar = $("span", progressBar);
    const total = reduceMotion.matches ? 0 : 2400;
    for (let i = 0; i < lines.length; i++) {
      say(...lines[i]);
      enemies.forEach((e, k) => { if (e.down && k % lines.length === i) e.g.classList.add("is-down"); });
      bar.style.transform = `scaleX(${(i + 1) / lines.length})`;
      if (total) await new Promise(r => setTimeout(r, total / lines.length));
    }
    enemies.forEach(e => e.down && e.g.classList.add("is-down"));
    progressBar.hidden = true; bar.style.transform = "scaleX(0)";

    // After-action report, then the fallen go on the wall.
    const plates = $("[data-aar-plates]"); plates.replaceChildren();
    const fresh = [];
    squad.forEach(m => {
      const st = statusOf(m);
      if (st === "kia") m.g.classList.add("is-down");
      const label = m.you ? "Commander (you)" : m.name;
      const what = st === "kia" ? "Killed in action, Red Tide (simulated)" : st === "wounded" ? `${Math.round(m.damage)}% damage. Two missions in the medbay` : "Returned to duty";
      plates.appendChild(plate({ name: label, what }, st === "kia" ? "kia" : st === "wounded" ? "wounded" : "", false));
      if (st === "kia" && !m.you) { const f = { name: m.name, what: `Red Tide, simulated ${new Date().toLocaleDateString()}` }; fallen.push(f); fresh.push(f); }
    });
    store.set("dv-wall", fallen);
    drawRoster(true);
    if (wall) drawWall(fresh);
    $("[data-aar-title]").textContent = win ? "Mission accomplished" : "Squad pulled back";
    const lost = squad.filter(m => statusOf(m) === "kia").length;
    $("[data-aar-why]").textContent = (win ? "The relay is down. " : "The relay still stands. ") +
      (lost ? `${lost} didn't come home. ` : "Everybody came home. ") + (reasons.length ? `${lost ? "Why" : "Close calls"}: ${reasons.slice(0, 2).join("; ")}.` : "Good cover and suppression kept the squad alive.");
    aar.hidden = false;
    setHint(lost ? "Their names are on the roll call wall below. Reset to try again." : "Reset to plan another op.");
    busy = false; resetBtn.disabled = false;
  };
  runBtn.addEventListener("click", run);
  resetBtn.addEventListener("click", () => { if (!busy) { setup(); selectOrder("move"); } });
  setup();

  // A link to a section (#ops, #roll) lands on it once the page has laid out.
  if (location.hash && location.hash.length > 1) {
    const target = document.getElementById(location.hash.slice(1));
    if (target) requestAnimationFrame(() => target.scrollIntoView({ behavior: "instant", block: "start" }));
  }

  // ?autoplay plays one sensible plan through the real planner: into cover, suppress the relay, flank the right.
  if (new URLSearchParams(location.search).has("autoplay")) {
    const plan = [["move", { x: 560, y: 405 }], ["suppress", { x: 740, y: 330 }], ["flank", { x: 760, y: 455 }]];
    plan.forEach(([o, p]) => { selectOrder(o); issue(p); });
    setTimeout(async () => { await run(); if (location.hash) document.getElementById(location.hash.slice(1))?.scrollIntoView({ behavior: "instant" }); }, 600);
  }
})();
