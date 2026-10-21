/* Delta-V: Dominion: the cinematic layer.
   1. The statement inks itself in, word by word, as it scrolls through the middle of the screen.
   2. Fly home: a pinned stage where scroll scrubs 139 real in-engine frames of the flight into the Kestrel.
   3. The worlds: a 3D war table (three.js) you can drag; the tab list beside it is the accessible control.
   Everything degrades: no script or reduced motion shows stills and a plain list. */
(() => {
  "use strict";
  const reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));

  /** Where the element is in its scroll run: 0 when its top reaches the top of the screen, 1 when its end does. */
  function runProgress(el) {
    const r = el.getBoundingClientRect();
    const run = r.height - window.innerHeight;
    return run > 0 ? clamp(-r.top / run, 0, 1) : 0;
  }

  // ------------------------------------------------------------------------------------------------
  // 1. The statement
  // ------------------------------------------------------------------------------------------------
  const say = document.querySelector("[data-say]");
  const sayWords = [];
  if (say && !reduce) {
    const keys = new Set(["name.", "real.", "rules.", "remembers"]);
    const text = say.textContent.trim();
    say.setAttribute("aria-label", text);
    say.textContent = "";
    for (const word of text.split(/\s+/)) {
      const span = document.createElement("span");
      span.className = "w" + (keys.has(word) ? " is-key" : "");
      span.textContent = word;
      span.setAttribute("aria-hidden", "true");
      say.append(span, " ");
      sayWords.push(span);
    }
  }
  function tickSay() {
    if (!sayWords.length) return;
    const r = say.getBoundingClientRect();
    const vh = window.innerHeight;
    // Inks in between the paragraph's top reaching 85% of the screen and its bottom reaching 45%.
    const p = clamp((vh * 0.85 - r.top) / (r.height + vh * 0.4), 0, 1);
    const lit = p * sayWords.length;
    sayWords.forEach((w, i) => w.style.setProperty("--o", (0.16 + 0.84 * clamp(lit - i, 0, 1)).toFixed(3)));
  }

  // ------------------------------------------------------------------------------------------------
  // 2. Fly home
  // ------------------------------------------------------------------------------------------------
  const flight = document.querySelector("[data-flight]");
  const FRAMES = 139;
  const frames = new Array(FRAMES);
  let loaded = 0;
  let flightStarted = false;
  let lastDrawn = -1;
  const canvas = flight && flight.querySelector("[data-flight-canvas]");
  const ctx = canvas && canvas.getContext("2d");
  const beats = flight ? [...flight.querySelectorAll("[data-flight-beats] li")] : [];
  const meter = flight && flight.querySelector("[data-flight-meter]");

  function loadFrames() {
    if (flightStarted) return;
    flightStarted = true;
    // First, last and every 8th frame first, so a fast scroll always has something close to show.
    const order = [];
    for (let step of [8, 4, 2, 1]) for (let i = 0; i < FRAMES; i += step) if (!order.includes(i)) order.push(i);
    order.push(FRAMES - 1);
    let next = 0;
    const pump = () => {
      if (next >= order.length) return;
      const i = order[next++];
      if (frames[i]) return pump();
      const img = new Image();
      img.decoding = "async";
      img.onload = () => { frames[i] = img; loaded++; if (lastDrawn < 0) drawFlight(true); pump(); };
      img.onerror = pump;
      img.src = "site/flight/f" + String(i).padStart(3, "0") + ".webp";
    };
    for (let k = 0; k < 4; k++) pump();   // four at a time
  }

  function sizeCanvas() {
    if (!canvas) return;
    const dpr = Math.min(window.devicePixelRatio || 1, 2);
    canvas.width = Math.round(canvas.clientWidth * dpr);
    canvas.height = Math.round(canvas.clientHeight * dpr);
    lastDrawn = -1;
  }

  function nearestLoaded(i) {
    for (let d = 0; d < FRAMES; d++) {
      if (frames[i - d]) return i - d;
      if (frames[i + d]) return i + d;
    }
    return -1;
  }

  function drawFlight(force) {
    if (!flight || !flight.classList.contains("is-scrub")) return;
    if (canvas.width !== Math.round(canvas.clientWidth * Math.min(window.devicePixelRatio || 1, 2))) sizeCanvas();
    const p = runProgress(flight);
    const want = Math.round(p * (FRAMES - 1));
    const i = nearestLoaded(want);
    if (meter) meter.style.transform = "scaleX(" + p.toFixed(4) + ")";
    beats.forEach((li, k) => {
      const from = parseFloat(li.dataset.at);
      const to = k + 1 < beats.length ? parseFloat(beats[k + 1].dataset.at) : 1.01;
      li.classList.toggle("is-on", p >= from && p < to);
    });
    if (i < 0 || (i === lastDrawn && !force)) return;
    lastDrawn = i;
    const img = frames[i];
    // Cover the stage, with a hair of overscan so the frames' letterbox edges never show.
    const cw = canvas.width, ch = canvas.height;
    const s = Math.max(cw / img.width, ch / img.height) * 1.04;
    const w = img.width * s, h = img.height * s;
    ctx.drawImage(img, (cw - w) / 2, (ch - h) / 2, w, h);
  }

  if (flight && canvas && !reduce) {
    flight.classList.add("is-scrub");
    sizeCanvas();
    new IntersectionObserver((entries, obs) => {
      if (entries.some((e) => e.isIntersecting)) { loadFrames(); obs.disconnect(); }
    }, { rootMargin: "150% 0px" }).observe(flight);
  }

  // ------------------------------------------------------------------------------------------------
  // 3. The worlds
  // ------------------------------------------------------------------------------------------------
  const WORLDS = [
    { name: "Mars", where: "Sol", rules: "lethal", color: "#b4532d", band: "#6e2c17", img: "outpost",
      ops: "Red Tide  ·  Ironstride", text: "CINDER's foundries took the red planet first. Relays on the ridges, and walkers the size of buildings striding the plains." },
    { name: "Gaia", where: "Sol", rules: "lethal", color: "#2f6fb0", band: "#3f8a4a", img: "planet_gaia",
      ops: "Breakwater  ·  Gaia Falls", text: "Home. The evacuation of Port Halden, and the hills outside the capital where the line has to hold." },
    { name: "Luna", where: "Sol", rules: "training", color: "#9a9a96", band: "#6c6c69", img: "bootcamp",
      ops: "Boot Camp", text: "The Copernicus range, six weeks before the uprising. Live rounds, live targets, and Halloran watching." },
    { name: "Europa", where: "Jupiter", rules: "lethal", color: "#d6e6ef", band: "#9ab4c4", img: "planet_europa",
      ops: "Glass Ocean", text: "Ice over an ocean nobody has seen. CINDER is listening to something down there." },
    { name: "Io", where: "Jupiter", rules: "lethal", color: "#d8b73a", band: "#9a5b1c", img: "planet_io",
      ops: "Forgefall", text: "Lava channels and the forge that builds the Bulwarks. Fall in and you won't need a medbay." },
    { name: "Ceres", where: "The Belt", rules: "lethal", color: "#5d6168", band: "#3a3d42", img: "planet_ceres",
      ops: "Ghost Signal", text: "A night mining town gone dark. The Hollow Men are using it as a relay." },
    { name: "Proxima b", where: "Alpha Centauri", rules: "lethal", color: "#7b4a32", band: "#3e5a2a", img: "planet_proxima",
      ops: "Cold Wake", text: "Four thousand colonists behind the trees, and one landing field between them and home." },
    { name: "Brasshaven", where: "Wolf 359", rules: "restrained", color: "#b07a2f", band: "#5e3a17", img: "brass_street",
      ops: "Steamheart", text: "Eighty years off the Lattice, rebuilt on steam and brass. Civilians in every street, and the Guild owns the sky." },
    { name: "Hollyhock Vale", where: "Tau Ceti", rules: "gentle", color: "#7cc06a", band: "#f2a7c3", img: "holly_square",
      ops: "Lantern Night", text: "A village raised by a kindly caretaker AI. Rifles fire foam here, and hurting anyone sends you home." },
  ];
  const RULES = { lethal: "Lethal rules", restrained: "Restrained rules", gentle: "Non-lethal rules", training: "Training" };

  const list = document.querySelector("[data-worlds-list]");
  const card = document.querySelector("[data-world-card]");
  let selected = 0;
  let onSelect = () => {};

  function select(i, fromUser) {
    selected = (i + WORLDS.length) % WORLDS.length;
    const w = WORLDS[selected];
    list.querySelectorAll("[role=tab]").forEach((t, k) => {
      t.setAttribute("aria-selected", String(k === selected));
      t.tabIndex = k === selected ? 0 : -1;
    });
    card.querySelector("[data-world-where]").textContent = w.where;
    card.querySelector("[data-world-name]").textContent = w.name;
    const rules = card.querySelector("[data-world-rules]");
    rules.textContent = RULES[w.rules];
    rules.className = "world-rules " + w.rules;
    card.querySelector("[data-world-text]").textContent = w.text;
    card.querySelector("[data-world-ops]").textContent = "Operations: " + w.ops;
    const img = card.querySelector("[data-world-img]");
    img.src = "site/img/" + w.img + "-640.webp";
    img.alt = "In-engine view of " + w.name;
    if (!reduce) {
      card.classList.remove("is-swapping");
      void card.offsetWidth;
      card.classList.add("is-swapping");
    }
    card.setAttribute("aria-labelledby", "world-tab-" + selected);
    onSelect(selected, fromUser);
  }

  if (list && card) {
    WORLDS.forEach((w, i) => {
      const b = document.createElement("button");
      b.type = "button";
      b.className = "world-tab";
      b.id = "world-tab-" + i;
      b.setAttribute("role", "tab");
      b.style.setProperty("--c", w.color);
      b.innerHTML = "<i aria-hidden='true'></i>";
      b.append(w.name);
      b.addEventListener("click", () => select(i, true));
      list.append(b);
    });
    list.addEventListener("keydown", (e) => {
      const d = { ArrowRight: 1, ArrowDown: 1, ArrowLeft: -1, ArrowUp: -1 }[e.key];
      if (e.key === "Home" || e.key === "End" || d) {
        e.preventDefault();
        select(e.key === "Home" ? 0 : e.key === "End" ? WORLDS.length - 1 : selected + d, true);
        list.querySelectorAll("[role=tab]")[selected].focus();
      }
    });
    select(0, false);
  }

  /** A planet's skin, painted on a canvas: soft latitude bands, a little noise, a lit limb comes from the scene. */
  function planetTexture(w, seed) {
    const c = document.createElement("canvas");
    c.width = 256; c.height = 128;
    const g = c.getContext("2d");
    g.fillStyle = w.color;
    g.fillRect(0, 0, 256, 128);
    let s = seed * 9301 + 49297;
    const rnd = () => ((s = (s * 9301 + 49297) % 233280) / 233280);
    for (let k = 0; k < 18; k++) {
      g.globalAlpha = 0.12 + rnd() * 0.22;
      g.fillStyle = k % 3 ? w.band : "#ffffff";
      const y = rnd() * 128, h = 2 + rnd() * 14;
      g.fillRect(0, y, 256, h);
    }
    for (let k = 0; k < 140; k++) {
      g.globalAlpha = 0.08 + rnd() * 0.18;
      g.fillStyle = rnd() < 0.5 ? w.band : "#000000";
      g.beginPath();
      g.arc(rnd() * 256, rnd() * 128, 1 + rnd() * 9, 0, Math.PI * 2);
      g.fill();
    }
    g.globalAlpha = 1;
    return c;
  }

  function startWarTable() {
    const view = document.querySelector("[data-worlds-view]");
    const cvs = document.querySelector("[data-worlds-canvas]");
    const THREE = window.THREE;
    if (!view || !cvs || !THREE) {
      if (view) view.classList.add("no-3d");
      return;
    }
    let renderer;
    try {
      renderer = new THREE.WebGLRenderer({ canvas: cvs, antialias: true, alpha: false });
    } catch (e) {
      view.classList.add("no-3d");
      return;
    }
    renderer.setPixelRatio(Math.min(window.devicePixelRatio || 1, 2));
    renderer.setClearColor(0x07080a, 1);
    const scene = new THREE.Scene();
    const camera = new THREE.PerspectiveCamera(34, 4 / 3, 0.1, 200);
    camera.position.set(0, 2.2, 11.5);
    camera.lookAt(0, 0.2, 0);

    // Light: a warm key like the deck's amber, a cool rim like the Kestrel's cyan.
    scene.add(new THREE.AmbientLight(0x404650, 0.9));
    const key = new THREE.DirectionalLight(0xffd59a, 1.35);
    key.position.set(-6, 5, 8);
    scene.add(key);
    const rim = new THREE.DirectionalLight(0x5cc8ff, 0.9);
    rim.position.set(6, -1, -6);
    scene.add(rim);

    // Stars: a shell of points.
    const starGeo = new THREE.BufferGeometry();
    const pos = new Float32Array(1600 * 3);
    for (let i = 0; i < 1600; i++) {
      const v = new THREE.Vector3().randomDirection ? new THREE.Vector3().randomDirection() : new THREE.Vector3(Math.random() - 0.5, Math.random() - 0.5, Math.random() - 0.5).normalize();
      v.multiplyScalar(60 + Math.random() * 40);
      pos.set([v.x, v.y, v.z], i * 3);
    }
    starGeo.setAttribute("position", new THREE.BufferAttribute(pos, 3));
    scene.add(new THREE.Points(starGeo, new THREE.PointsMaterial({ color: 0xcfd6dd, size: 0.18, sizeAttenuation: true })));

    // The table: a thin amber ring, and the worlds around it.
    const table = new THREE.Group();
    scene.add(table);
    const ring = new THREE.Mesh(new THREE.TorusGeometry(4.2, 0.012, 8, 160), new THREE.MeshBasicMaterial({ color: 0xf2b01e, transparent: true, opacity: 0.55 }));
    ring.rotation.x = Math.PI / 2;
    table.add(ring);
    const R = 4.2;
    const planets = WORLDS.map((w, i) => {
      const a = (i / WORLDS.length) * Math.PI * 2;
      const size = w.name === "Gaia" ? 0.62 : w.name === "Luna" || w.name === "Ceres" ? 0.38 : 0.52;
      const tex = new THREE.CanvasTexture(planetTexture(w, i + 1));
      const mesh = new THREE.Mesh(new THREE.SphereGeometry(size, 48, 32), new THREE.MeshStandardMaterial({ map: tex, roughness: 0.85, metalness: 0.05 }));
      mesh.position.set(Math.sin(a) * R, 0, Math.cos(a) * R);
      mesh.userData.index = i;
      mesh.rotation.z = 0.25;
      // A faint halo: a slightly bigger back-faced shell in the world's own colour.
      const halo = new THREE.Mesh(new THREE.SphereGeometry(size * 1.12, 32, 24),
        new THREE.MeshBasicMaterial({ color: new THREE.Color(w.color), transparent: true, opacity: 0.16, side: THREE.BackSide }));
      mesh.add(halo);
      table.add(mesh);
      return mesh;
    });
    // The marker under the selected world.
    const marker = new THREE.Mesh(new THREE.RingGeometry(0.78, 0.84, 64), new THREE.MeshBasicMaterial({ color: 0xf2b01e, side: THREE.DoubleSide }));
    marker.rotation.x = -Math.PI / 2;
    marker.position.set(0, -0.72, R);
    scene.add(marker);

    // The table turns on a spring toward the selected world; dragging pushes it directly.
    let angle = 0, velocity = 0, target = 0;
    const stiffness = 60, damping = 14;
    const angleFor = (i) => -(i / WORLDS.length) * Math.PI * 2;
    const nearestTarget = (want) => {
      const t = angleFor(selected);
      const turns = Math.round((want - t) / (Math.PI * 2));
      return t + turns * Math.PI * 2;
    };
    onSelect = (i) => { target = nearestTarget(angle); if (reduce) { angle = target; velocity = 0; } };
    target = angleFor(selected);
    angle = target;

    let dragging = false, lastX = 0, dragMoved = 0;
    view.addEventListener("pointerdown", (e) => {
      dragging = true; dragMoved = 0; lastX = e.clientX;
      view.setPointerCapture(e.pointerId);
      view.classList.add("is-dragging");
    });
    view.addEventListener("pointermove", (e) => {
      if (!dragging) return;
      const dx = e.clientX - lastX;
      lastX = e.clientX;
      dragMoved += Math.abs(dx);
      angle += dx * 0.008;
      velocity = dx * 0.5;
    });
    const raycaster = new THREE.Raycaster();
    const endDrag = (e) => {
      if (!dragging) return;
      dragging = false;
      view.classList.remove("is-dragging");
      if (dragMoved < 6) {
        // A click: pick the world under the pointer.
        const r = cvs.getBoundingClientRect();
        raycaster.setFromCamera({ x: ((e.clientX - r.left) / r.width) * 2 - 1, y: -((e.clientY - r.top) / r.height) * 2 + 1 }, camera);
        const hit = raycaster.intersectObjects(planets, false)[0];
        if (hit) { select(hit.object.userData.index, true); return; }
      }
      // Snap to the world now facing us.
      const steps = Math.round((-angle / (Math.PI * 2)) * WORLDS.length);
      select(((steps % WORLDS.length) + WORLDS.length) % WORLDS.length, true);
    };
    view.addEventListener("pointerup", endDrag);
    view.addEventListener("pointercancel", endDrag);

    function resize() {
      const w = view.clientWidth, h = view.clientHeight;
      renderer.setSize(w, h, false);
      camera.aspect = w / h;
      camera.position.z = w / h < 1.2 ? 17 : 13.6;
      camera.updateProjectionMatrix();
    }
    resize();
    window.addEventListener("resize", resize);

    let visible = true;
    new IntersectionObserver((es) => { visible = es[0].isIntersecting; if (visible) last = performance.now(); }).observe(view);
    let last = performance.now();
    function frame(now) {
      requestAnimationFrame(frame);
      if (!visible) return;
      const dt = Math.min(0.05, (now - last) / 1000);
      last = now;
      if (!dragging && !reduce) {
        const force = (target - angle) * stiffness - velocity * damping;
        velocity += force * dt;
        angle += velocity * dt;
      }
      table.rotation.y = angle;
      planets.forEach((p, i) => {
        p.rotation.y += dt * (reduce ? 0 : 0.25);
        const s = i === selected ? 1.25 : 1;
        p.scale.setScalar(p.scale.x + (s - p.scale.x) * Math.min(1, dt * 8));
      });
      const bob = reduce ? 0 : Math.sin(now / 900) * 0.04;
      marker.position.y = -0.72 + bob;
      renderer.render(scene, camera);
    }
    requestAnimationFrame(frame);
  }

  // ------------------------------------------------------------------------------------------------
  // One scroll loop for the scroll-linked pieces.
  // ------------------------------------------------------------------------------------------------
  let ticking = false;
  function onScroll() {
    if (ticking) return;
    ticking = true;
    requestAnimationFrame(() => { ticking = false; tickSay(); drawFlight(false); });
  }
  window.addEventListener("scroll", onScroll, { passive: true });
  window.addEventListener("resize", () => { sizeCanvas(); drawFlight(true); tickSay(); });
  tickSay();

  // three.js arrives as a separate deferred script; it is in place by the time the page has loaded.
  if (document.readyState === "complete") startWarTable();
  else window.addEventListener("load", startWarTable);
})();
