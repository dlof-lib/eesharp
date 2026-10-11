// ───────────── مكوّنات الويب (100 مفهوم): مساعدات وأنماط CSS وسكربتات — تُضمَّن قبل المنفّذ ─────────────
// كل عائلة مكوّنات لها بادئة صنف (ee-l- ee-t- ...) ولا تُضاف أنماطها للصفحة إلا إن استُعملت.

std::string hs(const Value& v) { return htmlOf(v); }                       // محتوى (يُهرَّب النص العادي)
std::string ha(const Value& v) { return htmlEsc(toStr(v)); }              // قيمة صفة
std::string hu(const Value& v) { return htmlEsc(safeUrl(toStr(v))); }     // رابط آمن
std::string hrest(const Args& a, size_t from) {
  std::string o;
  for (size_t i = from; i < a.size(); i++) o += htmlOf(a[i]);
  return o;
}
std::string tg(const char* tag, const std::string& cls, const std::string& inner, const std::string& extra = "") {
  std::string o = std::string("<") + tag;
  if (!cls.empty()) o += " class=\"" + cls + "\"";
  o += extra + ">";
  if (htmlVoid(tag)) return o;
  return o + inner + "</" + tag + ">";
}
int webUid() {
  static int n = 0;
  return ++n;
}
// اسم النوع بالعربية → صنف اللون
std::string kindClass(const std::string& k) {
  if (k == "نجاح") return "ok";
  if (k == "تحذير") return "warn";
  if (k == "خطر") return "danger";
  if (k == "معلومة") return "info";
  if (k == "رمادي") return "gray";
  return "primary";
}
void needArgs(const Args& a, size_t n, const char* usage, int ln) {
  if (a.size() < n) fail(ln, std::string("الاستعمال: ") + usage);
}
// عناصر قائمة → قائمة HTML (كل عنصر قد يكون نصاً أو عنصراً جاهزاً)
std::vector<Value> listOf(const Value& v, const char* usage, int ln) {
  if (v.t != VT::List) fail(ln, std::string("الاستعمال: ") + usage);
  return *v.list;
}
// زوج [أ، ب] من قائمة
const std::vector<Value>& pairOf(const Value& v, size_t n, const char* usage, int ln) {
  if (v.t != VT::List || v.list->size() < n) fail(ln, std::string("الاستعمال: ") + usage);
  return *v.list;
}

// ── أنماط CSS لكل عائلة ──
struct WebChunk { const char* trigger; const char* css; };
static const WebChunk kWebChunks[] = {
{"ee-l-", R"CSS(
.ee-l-row{display:flex;flex-wrap:wrap;gap:16px;align-items:center}
.ee-l-col{display:flex;flex-direction:column;gap:12px}
.ee-l-cols{display:grid;gap:20px;margin:16px 0}
.ee-l-cols.c2{grid-template-columns:repeat(2,1fr)}.ee-l-cols.c3{grid-template-columns:repeat(3,1fr)}
@media (max-width:720px){.ee-l-cols.c2,.ee-l-cols.c3{grid-template-columns:1fr}}
.ee-l-center{display:flex;flex-direction:column;align-items:center;text-align:center;gap:10px;margin:0 auto}
.ee-l-narrow{max-width:640px;margin:0 auto;padding:0 18px}
.ee-l-side{display:grid;grid-template-columns:240px 1fr;gap:28px;max-width:1100px;margin:0 auto;padding:20px 18px}
.ee-l-side>aside{position:sticky;top:70px;align-self:start}
@media (max-width:760px){.ee-l-side{grid-template-columns:1fr}.ee-l-side>aside{position:static}}
)CSS"},
{"ee-t-", R"CSS(
.ee-t-sub{color:var(--muted);font-size:1.1rem;margin:-.2em 0 1em}
.ee-t-lead{font-size:1.25rem;line-height:1.9;color:var(--fg)}
.ee-t-muted{color:var(--muted)}
.ee-t-pull{margin:1.6em 0;padding:.4em 1.2em;border-inline-start:5px solid var(--accent);font-size:1.4rem;line-height:1.7;font-weight:600}
.ee-t-pull figcaption{font-size:.95rem;font-weight:400;color:var(--muted);margin-top:.5em}
.ee-t-fancy{position:relative;padding-bottom:.45em}
.ee-t-fancy::after{content:"";position:absolute;inset-inline-start:0;bottom:0;width:64px;height:4px;border-radius:4px;background:linear-gradient(90deg,var(--accent),var(--c2))}
mark{background:color-mix(in srgb,var(--c2) 35%,transparent);color:inherit;padding:.05em .3em;border-radius:5px}
abbr[title]{text-decoration:underline dotted;cursor:help}
kbd{font:600 .85em ui-monospace,monospace;background:var(--card);border:1px solid var(--line);border-bottom-width:3px;border-radius:6px;padding:.1em .5em}
)CSS"},
{"ee-a-", R"CSS(
.ee-a-alert{display:flex;gap:12px;align-items:flex-start;padding:14px 16px;border-radius:12px;border:1px solid color-mix(in srgb,var(--k) 40%,transparent);background:color-mix(in srgb,var(--k) 11%,var(--card));margin:14px 0}
.ee-a-alert .i{font-size:1.3rem;line-height:1.6}.ee-a-alert.info{--k:var(--info)}.ee-a-alert.ok{--k:var(--ok)}.ee-a-alert.warn{--k:var(--warn)}.ee-a-alert.danger{--k:var(--danger)}
.ee-a-note{margin:16px 0;padding:14px 18px;border-radius:12px;background:color-mix(in srgb,var(--c2) 12%,var(--card));border:1px dashed color-mix(in srgb,var(--c2) 55%,transparent)}
.ee-a-note>strong{display:block;margin-bottom:.2em}
.ee-a-badge{display:inline-block;padding:.08em .7em;border-radius:99px;font-size:.82rem;font-weight:700;line-height:1.7;color:#fff;background:var(--k)}
.ee-a-badge.primary{--k:var(--accent);color:var(--accent-fg)}.ee-a-badge.ok{--k:var(--ok)}.ee-a-badge.warn{--k:var(--warn)}.ee-a-badge.danger{--k:var(--danger)}.ee-a-badge.info{--k:var(--info)}.ee-a-badge.gray{--k:var(--muted)}
.ee-a-dot{display:inline-flex;align-items:center;gap:.5em}.ee-a-dot::before{content:"";width:.7em;height:.7em;border-radius:50%;background:var(--k);box-shadow:0 0 0 4px color-mix(in srgb,var(--k) 22%,transparent)}
.ee-a-dot.ok{--k:var(--ok)}.ee-a-dot.warn{--k:var(--warn)}.ee-a-dot.danger{--k:var(--danger)}.ee-a-dot.info,.ee-a-dot.primary{--k:var(--info)}.ee-a-dot.gray{--k:var(--muted)}
.ee-a-announce{background:var(--accent);color:var(--accent-fg);text-align:center;padding:.6em 18px;font-weight:600;font-size:.95rem}
.ee-a-announce a{color:inherit}
.ee-a-empty{text-align:center;padding:48px 20px;border:2px dashed var(--line);border-radius:var(--radius);color:var(--muted)}
.ee-a-empty .i{font-size:3rem;display:block;margin-bottom:.2em}.ee-a-empty h3{margin:.2em 0;color:var(--fg)}
)CSS"},
{"ee-b-", R"CSS(
.ee-b-btn{display:inline-block;font:inherit;font-weight:700;text-decoration:none;cursor:pointer;border:2px solid var(--k);background:var(--k);color:#fff;border-radius:11px;padding:.55em 1.3em;transition:transform .12s,filter .12s,box-shadow .12s;line-height:1.5}
.ee-b-btn:hover{filter:brightness(1.08);transform:translateY(-1px);box-shadow:0 6px 16px rgba(0,0,0,.14)}
.ee-b-btn.primary{--k:var(--accent);color:var(--accent-fg)}.ee-b-btn.second{--k:var(--muted)}.ee-b-btn.danger{--k:var(--danger)}.ee-b-btn.ok{--k:var(--ok)}
.ee-b-btn.outline{background:transparent;color:var(--k)}.ee-b-btn.outline.primary{color:var(--accent)}
.ee-b-btn.lg{font-size:1.15rem;padding:.7em 1.8em;border-radius:14px}
.ee-b-group{display:inline-flex;flex-wrap:wrap;gap:10px}
.ee-b-fab{position:fixed;inset-block-end:22px;inset-inline-start:22px;width:56px;height:56px;border-radius:50%;display:grid;place-items:center;background:var(--accent);color:var(--accent-fg);font-size:1.5rem;text-decoration:none;box-shadow:0 8px 22px rgba(0,0,0,.28);z-index:40}
)CSS"},
{"ee-c-", R"CSS(
.ee-c-card{background:var(--card);border:1px solid var(--line);border-radius:var(--radius);overflow:hidden;display:flex;flex-direction:column;box-shadow:0 1px 3px rgba(0,0,0,.05);transition:transform .15s,box-shadow .15s}
.ee-c-card:hover{transform:translateY(-3px);box-shadow:0 12px 28px rgba(0,0,0,.12)}
.ee-c-card .b{padding:16px 18px;display:flex;flex-direction:column;gap:.35em;flex:1}
.ee-c-card h3{margin:0;font-size:1.15rem}.ee-c-card p{margin:0;color:var(--muted);font-size:.95rem}
.ee-c-card>img,.ee-c-card>a>img{width:100%;aspect-ratio:16/10;object-fit:cover;display:block}
.ee-c-card .meta{font-size:.82rem;color:var(--muted)}
.ee-c-card .price{font-size:1.4rem;font-weight:800;color:var(--accent)}
.ee-c-card a.more{margin-top:auto;font-weight:700;text-decoration:none}
.ee-c-stat{padding:20px;text-align:center;gap:.1em}
.ee-c-stat .v{font-size:2.4rem;font-weight:800;line-height:1.2;color:var(--accent)}.ee-c-stat .l{color:var(--muted)}.ee-c-stat .i{font-size:1.6rem}
.ee-c-feat{padding:22px}
.ee-c-feat .i{width:52px;height:52px;border-radius:14px;display:grid;place-items:center;font-size:1.6rem;background:color-mix(in srgb,var(--accent) 14%,var(--card));margin-bottom:.4em}
.ee-c-quote{padding:22px}.ee-c-quote blockquote{border:0;padding:0;margin:0 0 .8em;font-size:1.05rem;color:var(--fg)}
.ee-c-quote blockquote::before{content:"”";display:block;font-size:3rem;line-height:.6;color:var(--accent)}
.ee-c-quote .who{display:flex;flex-direction:column}.ee-c-quote .who span{color:var(--muted);font-size:.9rem}
.ee-c-team{text-align:center;padding:22px}.ee-c-team img{width:96px;height:96px;border-radius:50%;object-fit:cover;margin:0 auto .6em;aspect-ratio:1}
.ee-c-price{padding:24px;text-align:center;position:relative}.ee-c-price.hot{border:2px solid var(--accent);box-shadow:0 12px 30px color-mix(in srgb,var(--accent) 25%,transparent)}
.ee-c-price .tag{position:absolute;top:-1px;inset-inline-end:16px;background:var(--accent);color:var(--accent-fg);font-size:.78rem;font-weight:700;padding:.2em .8em;border-radius:0 0 8px 8px}
.ee-c-price ul{list-style:none;padding:0;margin:.6em 0 1.2em;text-align:start}.ee-c-price li{padding:.35em 0;border-bottom:1px solid var(--line)}.ee-c-price li::before{content:"✓ ";color:var(--ok);font-weight:800}
.ee-c-link{padding:18px 20px;text-decoration:none;color:inherit}.ee-c-link:hover h3{color:var(--accent)}
)CSS"},
{"ee-s-", R"CSS(
.ee-s-sec{padding:56px 18px}.ee-s-sec>.in{max-width:1000px;margin:0 auto}
.ee-s-sec.tint{background:color-mix(in srgb,var(--accent) 7%,var(--bg))}.ee-s-sec.dark{background:#1d2a33;color:#eef3f6}.ee-s-sec.warm{background:color-mix(in srgb,var(--c2) 12%,var(--bg))}
.ee-s-head{text-align:center;max-width:640px;margin:0 auto 32px}.ee-s-head h2{margin:0 0 .3em;font-size:1.9rem}.ee-s-head p{margin:0;color:var(--muted)}
.ee-s-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(240px,1fr));gap:18px}
.ee-s-cta{text-align:center;padding:60px 18px;background:linear-gradient(135deg,var(--accent),color-mix(in srgb,var(--accent) 50%,#000));color:var(--accent-fg)}
.ee-s-cta h2{margin:0 0 .3em;font-size:2rem}.ee-s-cta p{margin:0 auto 1.4em;max-width:560px;opacity:.92}
.ee-s-cta .ee-b-btn{--k:#fff;background:#fff;color:#1d2a33;border-color:#fff}
.ee-s-faq details{background:var(--card);border:1px solid var(--line);border-radius:12px;margin:10px 0;padding:0 18px}
.ee-s-faq summary{cursor:pointer;font-weight:700;padding:14px 0;list-style:none;display:flex;justify-content:space-between;gap:12px}
.ee-s-faq summary::after{content:"＋";color:var(--accent)}.ee-s-faq details[open] summary::after{content:"－"}.ee-s-faq details>div{padding:0 0 14px;color:var(--muted)}
.ee-s-hero2{display:grid;grid-template-columns:1.1fr 1fr;gap:36px;align-items:center;max-width:1050px;margin:0 auto;padding:56px 18px}
.ee-s-hero2 h1{font-size:2.3rem;margin:0 0 .3em}.ee-s-hero2 p{color:var(--muted);font-size:1.15rem}.ee-s-hero2 img{width:100%;border-radius:20px;box-shadow:0 20px 50px rgba(0,0,0,.2)}
@media (max-width:760px){.ee-s-hero2{grid-template-columns:1fr;text-align:center}}
.ee-s-hero3{padding:72px 18px;max-width:820px;margin:0 auto}.ee-s-hero3 h1{font-size:2.6rem;margin:0 0 .3em;line-height:1.25}.ee-s-hero3 p{font-size:1.2rem;color:var(--muted);margin:0 0 1.4em}
)CSS"},
{"ee-n-", R"CSS(
.ee-n-bar{display:flex;align-items:center;gap:20px;padding:12px 18px;background:var(--card);border-bottom:1px solid var(--line);position:sticky;top:0;z-index:30;flex-wrap:wrap}
.ee-n-logo{font-weight:800;font-size:1.25rem;text-decoration:none;color:var(--fg);display:inline-flex;gap:.4em;align-items:center}.ee-n-logo b{color:var(--accent)}
.ee-n-links{display:flex;gap:6px 20px;margin-inline-start:auto;flex-wrap:wrap}.ee-n-links a{text-decoration:none;font-weight:600;color:var(--fg);padding:.2em 0;border-bottom:2px solid transparent}.ee-n-links a:hover{color:var(--accent);border-color:var(--accent)}
.ee-n-t{display:none}.ee-n-burger{display:none;margin-inline-start:auto;font-size:1.6rem;cursor:pointer;line-height:1}
@media (max-width:720px){.ee-n-burger{display:block}.ee-n-links{display:none;width:100%;flex-direction:column;margin:0;padding:6px 0}.ee-n-t:checked~.ee-n-links{display:flex}}
.ee-n-crumbs{display:flex;flex-wrap:wrap;gap:.4em;list-style:none;padding:0;margin:12px 0;font-size:.92rem;color:var(--muted)}
.ee-n-crumbs li+li::before{content:"‹";margin-inline-end:.4em}.ee-n-crumbs a{text-decoration:none}.ee-n-crumbs li:last-child{color:var(--fg);font-weight:600}
.ee-n-pages{display:flex;gap:6px;justify-content:center;list-style:none;padding:0;margin:20px 0;flex-wrap:wrap}.ee-n-pages a,.ee-n-pages span{display:grid;place-items:center;min-width:2.4em;height:2.4em;padding:0 .5em;border-radius:10px;border:1px solid var(--line);text-decoration:none;background:var(--card);color:var(--fg)}
.ee-n-pages .cur{background:var(--accent);color:var(--accent-fg);border-color:var(--accent);font-weight:700}
.ee-n-tabs .tl{display:flex;gap:4px;border-bottom:2px solid var(--line);overflow-x:auto}.ee-n-tabs .tl button{background:none;color:var(--muted);border-radius:10px 10px 0 0;padding:.6em 1.1em;font-weight:700;border-bottom:3px solid transparent;margin-bottom:-2px;white-space:nowrap}
.ee-n-tabs .tl button.on{color:var(--accent);border-color:var(--accent)}.ee-n-tabs .tp{padding:18px 4px}.ee-n-tabs .tp[hidden]{display:none}
.ee-n-side{list-style:none;padding:0;margin:0;display:flex;flex-direction:column;gap:2px}.ee-n-side a{display:block;padding:.55em .9em;border-radius:10px;text-decoration:none;color:var(--fg)}.ee-n-side a:hover{background:color-mix(in srgb,var(--accent) 12%,var(--card));color:var(--accent)}
.ee-n-foot{background:var(--card);border-top:1px solid var(--line);margin-top:40px}.ee-n-foot .in{max-width:1000px;margin:0 auto;padding:36px 18px 20px;display:grid;grid-template-columns:1.4fr repeat(auto-fit,minmax(150px,1fr));gap:28px}
.ee-n-foot h4{margin:0 0 .6em}.ee-n-foot ul{list-style:none;padding:0;margin:0}.ee-n-foot li{margin:.3em 0}.ee-n-foot a{text-decoration:none;color:var(--muted)}.ee-n-foot a:hover{color:var(--accent)}
.ee-n-foot .copy{max-width:1000px;margin:0 auto;padding:14px 18px 22px;border-top:1px solid var(--line);color:var(--muted);font-size:.9rem;text-align:center}
@media (max-width:720px){.ee-n-foot .in{grid-template-columns:1fr 1fr}.ee-n-foot .in>div:first-child{grid-column:1/-1}}
.ee-n-top{position:fixed;inset-block-end:22px;inset-inline-end:22px;width:46px;height:46px;border-radius:50%;display:grid;place-items:center;background:var(--card);border:1px solid var(--line);box-shadow:0 6px 18px rgba(0,0,0,.18);text-decoration:none;font-size:1.2rem;color:var(--accent);z-index:40}
.ee-n-social{display:flex;gap:10px;flex-wrap:wrap}.ee-n-social a{padding:.35em .9em;border:1px solid var(--line);border-radius:99px;text-decoration:none;font-size:.92rem;background:var(--card);color:var(--fg)}.ee-n-social a:hover{border-color:var(--accent);color:var(--accent)}
)CSS"},
{"ee-u-", R"CSS(
.ee-u-acc details{background:var(--card);border:1px solid var(--line);border-radius:12px;margin:8px 0;overflow:hidden}
.ee-u-acc summary,.ee-u-det summary{cursor:pointer;font-weight:700;padding:13px 16px;list-style:none;display:flex;justify-content:space-between;gap:10px}
.ee-u-acc summary::after,.ee-u-det summary::after{content:"▾";color:var(--accent);transition:transform .2s}.ee-u-acc details[open] summary::after,.ee-u-det[open] summary::after{transform:rotate(180deg)}
.ee-u-acc details>div,.ee-u-det>div{padding:0 16px 14px;color:var(--muted)}
.ee-u-det{background:var(--card);border:1px solid var(--line);border-radius:12px;margin:10px 0;overflow:hidden}
.ee-u-theme{background:var(--card);color:var(--fg);border:1px solid var(--line)}
dialog.ee-u-modal{border:0;border-radius:18px;padding:0;max-width:min(520px,92vw);width:100%;background:var(--card);color:var(--fg);box-shadow:0 30px 80px rgba(0,0,0,.4)}
dialog.ee-u-modal::backdrop{background:rgba(10,15,20,.55);backdrop-filter:blur(3px)}
.ee-u-modal .h{display:flex;justify-content:space-between;align-items:center;padding:14px 18px;border-bottom:1px solid var(--line)}.ee-u-modal .h h3{margin:0}
.ee-u-modal .h button{background:none;color:var(--muted);font-size:1.4rem;padding:0 .4em}.ee-u-modal .bd{padding:16px 18px}
.ee-u-prog{margin:12px 0}.ee-u-prog .t{display:flex;justify-content:space-between;font-size:.92rem;margin-bottom:.3em}.ee-u-prog .tr{height:12px;background:var(--line);border-radius:99px;overflow:hidden}
.ee-u-prog .fl{height:100%;background:linear-gradient(90deg,var(--accent),var(--c2));border-radius:99px;transition:width .6s}
.ee-u-count{font-size:2.4rem;font-weight:800;color:var(--accent);text-align:center;display:block;line-height:1.2}
.ee-u-cd{display:flex;gap:12px;justify-content:center;flex-wrap:wrap;direction:ltr}.ee-u-cd div{min-width:78px;text-align:center;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:12px 8px}
.ee-u-cd b{display:block;font-size:1.9rem;line-height:1.2;color:var(--accent);font-variant-numeric:tabular-nums}.ee-u-cd span{font-size:.82rem;color:var(--muted)}
[data-tip]{position:relative;cursor:help;border-bottom:1px dotted var(--muted)}
[data-tip]::after{content:attr(data-tip);position:absolute;inset-inline-start:50%;bottom:calc(100% + 8px);transform:translateX(-50%) scale(.95);opacity:0;pointer-events:none;background:#1d2a33;color:#fff;font-size:.85rem;padding:.35em .8em;border-radius:8px;white-space:nowrap;transition:.15s;z-index:20}
[data-tip]:hover::after,[data-tip]:focus::after{opacity:1;transform:translateX(-50%) scale(1)}
)CSS"},
{"ee-d-", R"CSS(
.ee-d-tbl{width:100%;border-collapse:collapse;margin:1em 0;background:var(--card);border-radius:var(--radius);overflow:hidden;border:1px solid var(--line)}
.ee-d-tbl th{background:color-mix(in srgb,var(--accent) 16%,var(--card));text-align:start;padding:.7em 1em}.ee-d-tbl td{padding:.65em 1em;border-top:1px solid var(--line)}
.ee-d-tbl tbody tr:nth-child(even){background:color-mix(in srgb,var(--accent) 4%,var(--card))}.ee-d-tbl tbody tr:hover{background:color-mix(in srgb,var(--accent) 10%,var(--card))}
.ee-d-dl{display:grid;grid-template-columns:max-content 1fr;gap:.6em 1.4em;margin:1em 0}.ee-d-dl dt{font-weight:700;color:var(--accent)}.ee-d-dl dd{margin:0;color:var(--muted)}
@media (max-width:560px){.ee-d-dl{grid-template-columns:1fr}.ee-d-dl dd{margin-bottom:.6em}}
.ee-d-time{list-style:none;margin:1.2em 0;padding:0;border-inline-start:3px solid var(--line)}.ee-d-time li{position:relative;padding:0 1.4em 1.4em}
.ee-d-time li::before{content:"";position:absolute;inset-inline-start:-9px;top:.45em;width:15px;height:15px;border-radius:50%;background:var(--accent);border:3px solid var(--bg)}
.ee-d-time time{font-size:.85rem;color:var(--accent);font-weight:700}.ee-d-time h4{margin:.1em 0}.ee-d-time p{margin:0;color:var(--muted)}
.ee-d-stars{display:inline-block;font-size:1.4rem;letter-spacing:.1em;line-height:1;background:linear-gradient(270deg,#f2b84b calc(var(--v)/var(--n)*100%),var(--line) 0);-webkit-background-clip:text;background-clip:text;color:transparent}
.ee-d-chart{display:flex;gap:24px;align-items:center;flex-wrap:wrap;justify-content:center}.ee-d-chart svg{width:220px;height:220px;flex:none}.ee-d-line{width:100%;height:auto}
.ee-d-leg{list-style:none;padding:0;margin:0}.ee-d-leg li{display:flex;gap:.6em;align-items:center;margin:.3em 0}.ee-d-leg i{width:.9em;height:.9em;border-radius:3px;background:var(--k)}
.ee-d-meter{margin:12px 0}.ee-d-meter .t{display:flex;justify-content:space-between;margin-bottom:.3em;font-weight:600}.ee-d-meter .tr{height:10px;background:var(--line);border-radius:99px;overflow:hidden}.ee-d-meter .fl{height:100%;background:var(--accent);border-radius:99px}
.ee-d-kpi{text-align:center;padding:10px}.ee-d-kpi b{display:block;font-size:3rem;line-height:1.1;color:var(--accent);font-weight:800}.ee-d-kpi span{color:var(--muted)}
.ee-d-steps{list-style:none;counter-reset:s;padding:0;margin:1.2em 0;display:grid;gap:14px}.ee-d-steps li{counter-increment:s;display:grid;grid-template-columns:44px 1fr;gap:14px;align-items:start}
.ee-d-steps li::before{content:counter(s);width:44px;height:44px;border-radius:50%;display:grid;place-items:center;font-weight:800;background:var(--accent);color:var(--accent-fg)}
.ee-d-steps h4{margin:.1em 0}.ee-d-steps p{margin:0;color:var(--muted)}
.ee-d-check{list-style:none;padding:0;margin:1em 0}.ee-d-check li{padding:.4em 0 .4em 0;display:flex;gap:.6em;align-items:flex-start}.ee-d-check li::before{content:"○";color:var(--muted)}.ee-d-check li.on::before{content:"✔";color:var(--ok);font-weight:800}
)CSS"},
{"ee-m-", R"CSS(
.ee-m-gal{display:grid;grid-template-columns:repeat(auto-fill,minmax(180px,1fr));gap:10px;margin:1em 0}.ee-m-gal img{width:100%;aspect-ratio:1;object-fit:cover;border-radius:12px;transition:transform .2s}.ee-m-gal img:hover{transform:scale(1.03)}
.ee-m-av{border-radius:50%;object-fit:cover;aspect-ratio:1;display:inline-block;vertical-align:middle;border:3px solid var(--card);box-shadow:0 0 0 1px var(--line)}
.ee-m-icon{display:inline-grid;place-items:center;width:2.4em;height:2.4em;border-radius:12px;background:color-mix(in srgb,var(--accent) 14%,var(--card));font-size:1.3rem}
.ee-m-yt{position:relative;aspect-ratio:16/9;border-radius:16px;overflow:hidden;background:#000;margin:1em 0}.ee-m-yt iframe{position:absolute;inset:0;width:100%;height:100%;border:0}
.ee-m-vid{width:100%;border-radius:16px;background:#000;margin:1em 0}
.ee-m-fig{margin:1.2em 0}.ee-m-fig img{width:100%;border-radius:14px}.ee-m-fig figcaption{text-align:center;color:var(--muted);font-size:.92rem;margin-top:.5em}
.ee-m-wave{line-height:0;color:var(--accent);margin-top:-1px}.ee-m-wave svg{width:100%;height:60px;display:block}
)CSS"},
{"ee-f-", R"CSS(
.ee-f-g{margin:.9em 0}.ee-f-g label{display:block;font-weight:700;margin-bottom:.3em}
.ee-f-form{background:var(--card);border:1px solid var(--line);border-radius:var(--radius);padding:22px;max-width:480px;margin:1em auto}
.ee-f-form h3{margin:0 0 .6em}.ee-f-form button[type=submit]{width:100%;margin-top:.6em;padding:.75em}
.ee-f-chk{display:flex;gap:.6em;align-items:center;margin:.7em 0;cursor:pointer;font-weight:600}.ee-f-chk input{width:1.15em;height:1.15em;accent-color:var(--accent)}
.ee-f-radio{display:flex;flex-direction:column;gap:.4em;margin:.8em 0}
.ee-f-sw{display:flex;gap:.8em;align-items:center;margin:.8em 0;cursor:pointer;font-weight:600}.ee-f-sw input{appearance:none;width:3em;height:1.6em;border-radius:99px;background:var(--line);position:relative;transition:.2s;cursor:pointer;flex:none;padding:0;border:0}
.ee-f-sw input::after{content:"";position:absolute;top:.2em;inset-inline-start:.2em;width:1.2em;height:1.2em;border-radius:50%;background:#fff;transition:.2s;box-shadow:0 1px 3px rgba(0,0,0,.3)}.ee-f-sw input:checked{background:var(--accent)}.ee-f-sw input:checked::after{inset-inline-start:calc(100% - 1.4em)}
.ee-f-search{display:flex;gap:8px;max-width:520px;margin:1em auto}.ee-f-search input{flex:1}.ee-f-search button{flex:none}
.ee-f-news{display:flex;gap:8px;flex-wrap:wrap;justify-content:center;max-width:520px;margin:1em auto}.ee-f-news input{flex:1;min-width:200px}
)CSS"},
{"ee-x-", R"CSS(
.ee-x-theme{display:contents}.ee-x-gtext{background:linear-gradient(90deg,var(--g1),var(--g2));-webkit-background-clip:text;background-clip:text;color:transparent;font-weight:800}
.ee-x-grid{display:grid;gap:16px;margin:1em 0}.ee-x-stack{display:flex;flex-direction:column;gap:14px;margin:1em 0}
.ee-x-div{display:flex;align-items:center;gap:12px;color:var(--muted);margin:1.6em 0;font-size:.9rem}.ee-x-div::before,.ee-x-div::after{content:"";flex:1;height:1px;background:var(--line)}.ee-x-div.plain::after{display:none}
.ee-x-gbg{padding:44px 20px;border-radius:22px;margin:1.2em 0;color:#fff;background:var(--gr,linear-gradient(135deg,var(--g1),var(--g2)))}.ee-x-gbg.light,.ee-x-gcard.light{color:#1b1c1e}
.g-بنفسجي{--g1:#6d4fd6;--g2:#b04fc9}.g-برتقالي{--g1:#f08a3c;--g2:#e04d6b}.g-اخضر{--g1:#2e9e6e;--g2:#1c6f8c}.g-ازرق{--g1:#2f6fd0;--g2:#1fa5c9}.g-وردي{--g1:#e0589a;--g2:#f08a6c}.g-داكن{--g1:#1f2a37;--g2:#3b4a63}
.ee-x-glass{background:rgba(255,255,255,.16);border:1px solid rgba(255,255,255,.32);backdrop-filter:blur(14px);-webkit-backdrop-filter:blur(14px);border-radius:18px;padding:20px 22px;margin:12px 0;color:inherit;box-shadow:0 8px 30px rgba(0,0,0,.12)}
.ee-x-gcard{border-radius:var(--radius);padding:24px;color:#fff;background:var(--gr,linear-gradient(135deg,var(--g1),var(--g2)));box-shadow:0 10px 26px -10px var(--g2)}.ee-x-gcard h3{margin:0 0 .3em}.ee-x-gcard p{margin:0;opacity:.92}
.ee-x-marq{overflow:hidden;margin:1.2em 0;mask-image:linear-gradient(90deg,transparent,#000 8%,#000 92%,transparent)}.ee-x-marq>div{display:flex;gap:40px;width:max-content;animation:eex-m 26s linear infinite}.ee-x-marq span{font-weight:700;color:var(--muted);white-space:nowrap;font-size:1.1rem}
@keyframes eex-m{to{transform:translateX(50%)}}
.ee-x-slider{display:flex;gap:14px;overflow-x:auto;scroll-snap-type:x mandatory;padding:6px 2px 14px;margin:1em 0;scrollbar-width:thin}.ee-x-slider>*{flex:0 0 min(86%,340px);scroll-snap-align:center}
.ee-x-tags{display:flex;flex-wrap:wrap;gap:8px;margin:.8em 0}.ee-x-tags span{padding:.25em .9em;border-radius:99px;background:color-mix(in srgb,var(--accent) 12%,var(--card));border:1px solid color-mix(in srgb,var(--accent) 25%,var(--line));font-size:.88rem;font-weight:600}
.ee-x-by{display:flex;align-items:center;gap:12px;margin:.8em 0}.ee-x-by img{width:46px;height:46px;border-radius:50%;object-fit:cover}.ee-x-by b{display:block;line-height:1.3}.ee-x-by small{color:var(--muted)}
.ee-x-pros{list-style:none;padding:0;margin:1em 0;display:grid;gap:10px}.ee-x-pros li{display:flex;gap:12px;align-items:flex-start}.ee-x-pros .i{flex:none;width:2.2em;height:2.2em;display:grid;place-items:center;border-radius:10px;background:color-mix(in srgb,var(--accent) 14%,var(--card))}
.ee-x-cmp{width:100%;border-collapse:separate;border-spacing:0;margin:1em 0;background:var(--card);border:1px solid var(--line);border-radius:var(--radius);overflow:hidden;text-align:center}.ee-x-cmp th,.ee-x-cmp td{padding:.7em .6em;border-bottom:1px solid var(--line)}.ee-x-cmp th{background:color-mix(in srgb,var(--accent) 9%,var(--card))}.ee-x-cmp td:first-child,.ee-x-cmp th:first-child{text-align:start;font-weight:700}.ee-x-cmp tr:last-child td{border-bottom:0}.ee-x-cmp .y{color:var(--ok);font-weight:800}.ee-x-cmp .n{color:var(--muted)}
.ee-x-phone{width:min(260px,70%);aspect-ratio:9/18;margin:1.2em auto;border:10px solid #1b1c1e;border-radius:34px;background:var(--card);overflow:hidden;position:relative;box-shadow:0 18px 40px -14px rgba(0,0,0,.45)}.ee-x-phone::before{content:"";position:absolute;top:0;left:50%;translate:-50% 0;width:36%;height:16px;background:#1b1c1e;border-radius:0 0 12px 12px;z-index:1}.ee-x-phone>div{height:100%;overflow:auto;padding:26px 12px 12px;font-size:.9rem;line-height:1.6}
.ee-x-brw{border:1px solid var(--line);border-radius:12px;overflow:hidden;margin:1.2em 0;background:var(--card);box-shadow:0 12px 30px -16px rgba(0,0,0,.35)}.ee-x-brw .bar{display:flex;align-items:center;gap:6px;padding:10px 12px;background:color-mix(in srgb,var(--fg) 6%,var(--card));border-bottom:1px solid var(--line)}.ee-x-brw .bar i{width:11px;height:11px;border-radius:50%;background:#f1665b}.ee-x-brw .bar i:nth-child(2){background:#f4be4f}.ee-x-brw .bar i:nth-child(3){background:#5fc454}.ee-x-brw .bar span{flex:1;margin-inline-start:10px;padding:.15em .8em;border-radius:99px;background:var(--bg);font-size:.8rem;color:var(--muted);direction:ltr;text-align:start;overflow:hidden;white-space:nowrap;text-overflow:ellipsis}.ee-x-brw .bd{padding:16px}
.ee-x-code{margin:1.2em 0;border-radius:12px;background:#17191c;color:#e8e6e1;overflow:hidden;direction:ltr;text-align:left}.ee-x-code .h{display:flex;justify-content:space-between;align-items:center;padding:6px 8px 6px 14px;background:#22252a;font:600 .8rem system-ui;color:#9aa0a6}.ee-x-code .h button{padding:.2em .8em;font-size:.8rem;border-radius:8px;background:#2f343b;color:#e8e6e1;border:0;cursor:pointer}.ee-x-code pre{margin:0;padding:14px 16px;overflow:auto;font:.9rem/1.65 ui-monospace,Menlo,Consolas,monospace}
.ee-x-code pre,.ee-x-code code{background:transparent;color:inherit;padding-block:0;border:0;border-radius:0}.ee-x-code pre{padding:14px 16px}
.ee-x-term{margin:1.2em 0;border-radius:12px;background:#101214;color:#d5f5dc;padding:14px 16px;direction:ltr;text-align:left;font:.9rem/1.7 ui-monospace,Menlo,Consolas,monospace;overflow:auto}.ee-x-term div::before{content:"$ ";color:#6bd48c}
.ee-x-spin{display:inline-block;width:2.2rem;height:2.2rem;border:3px solid var(--line);border-top-color:var(--accent);border-radius:50%;animation:eex-s .8s linear infinite;vertical-align:middle}@keyframes eex-s{to{transform:rotate(360deg)}}
.ee-x-skel{display:grid;gap:10px;margin:1em 0}.ee-x-skel i{display:block;height:1em;border-radius:8px;background:linear-gradient(90deg,var(--line) 25%,color-mix(in srgb,var(--line) 50%,var(--card)) 50%,var(--line) 75%);background-size:200% 100%;animation:eex-k 1.3s infinite}.ee-x-skel i:last-child{width:60%}@keyframes eex-k{to{background-position:-200% 0}}
.ee-x-ring{display:inline-grid;place-items:center;position:relative;margin:.6em}.ee-x-ring svg{width:130px;height:130px;transform:rotate(-90deg)}.ee-x-ring b{position:absolute;font-size:1.5rem}.ee-x-ring small{position:absolute;margin-top:2.6em;color:var(--muted);font-size:.8rem}
.ee-x-hbar{display:grid;gap:12px;margin:1em 0}.ee-x-hbar .r{display:grid;grid-template-columns:90px 1fr 48px;gap:10px;align-items:center;font-size:.92rem}.ee-x-hbar .t{height:12px;border-radius:99px;background:var(--line);overflow:hidden}.ee-x-hbar .t i{display:block;height:100%;border-radius:99px;background:var(--accent)}.ee-x-hbar .r span:last-child{color:var(--muted);text-align:end}
.ee-x-spark{width:140px;height:40px;vertical-align:middle}
.ee-x-log{margin:1em 0;display:grid;gap:18px}.ee-x-log .e{border-inline-start:3px solid var(--accent);padding-inline-start:16px}.ee-x-log h4{margin:0}.ee-x-log time{color:var(--muted);font-size:.85rem}.ee-x-log ul{margin:.4em 0 0;padding-inline-start:1.2em}
.ee-x-contact{display:grid;gap:12px;background:var(--card);border:1px solid var(--line);border-radius:var(--radius);padding:20px;margin:1em 0}.ee-x-contact a,.ee-x-contact div{display:flex;gap:12px;align-items:center;text-decoration:none;color:inherit}.ee-x-contact b{width:2.2em;height:2.2em;display:grid;place-items:center;border-radius:10px;background:color-mix(in srgb,var(--accent) 14%,var(--card));font-weight:400}
.ee-x-hours{width:100%;max-width:420px;border-collapse:collapse;margin:1em 0}.ee-x-hours td{padding:.55em .2em;border-bottom:1px dashed var(--line)}.ee-x-hours td:last-child{text-align:end;color:var(--muted);direction:ltr}
.ee-x-logos{display:grid;grid-template-columns:repeat(auto-fit,minmax(110px,1fr));gap:12px;margin:1.2em 0;text-align:center}.ee-x-logos span{padding:.8em;border:1px dashed var(--line);border-radius:12px;font-weight:800;color:var(--muted);letter-spacing:.02em}
.ee-x-ar{position:relative;width:100%;overflow:hidden;border-radius:14px;margin:1em 0}.ee-x-ar>*{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}.ee-x-ar::before{content:"";display:block;padding-top:var(--r)}
.ee-x-share{display:flex;flex-wrap:wrap;gap:8px;margin:.8em 0}.ee-x-share a{padding:.4em 1em;border-radius:99px;border:1px solid var(--line);background:var(--card);text-decoration:none;font-weight:700;font-size:.9rem}.ee-x-share a:hover{border-color:var(--accent);color:var(--accent)}
.ee-x-reveal{opacity:0;transform:translateY(22px);transition:opacity .7s,transform .7s}.ee-x-reveal.in{opacity:1;transform:none}@media (prefers-reduced-motion:reduce){.ee-x-reveal{opacity:1;transform:none;transition:none}.ee-x-marq>div{animation:none}}
.ee-x-cm{display:flex;gap:12px;margin:1em 0}.ee-x-cm img{flex:none;width:44px;height:44px;border-radius:50%;object-fit:cover}.ee-x-cm .b{flex:1;background:var(--card);border:1px solid var(--line);border-radius:4px 14px 14px 14px;padding:10px 14px}.ee-x-cm small{color:var(--muted)}.ee-x-cm p{margin:.3em 0 0}
.ee-x-dd{position:relative;display:inline-block}.ee-x-dd summary{list-style:none;cursor:pointer;padding:.5em 1.1em;border:1px solid var(--line);border-radius:10px;background:var(--card);font-weight:700}.ee-x-dd summary::-webkit-details-marker{display:none}.ee-x-dd summary::after{content:" ▾"}.ee-x-dd div{position:absolute;z-index:20;min-width:190px;margin-top:6px;background:var(--card);border:1px solid var(--line);border-radius:12px;padding:6px;box-shadow:0 14px 30px rgba(0,0,0,.18)}.ee-x-dd div a{display:block;padding:.5em .8em;border-radius:8px;text-decoration:none;color:var(--fg)}.ee-x-dd div a:hover{background:color-mix(in srgb,var(--accent) 12%,var(--card))}
.ee-x-corner{position:relative;overflow:hidden;border-radius:var(--radius);border:1px solid var(--line);background:var(--card);padding:22px;margin:1em 0}.ee-x-corner>b{position:absolute;top:18px;inset-inline-end:-38px;transform:rotate(45deg);background:var(--danger);color:#fff;padding:.15em 3em;font-size:.8rem;box-shadow:0 2px 6px rgba(0,0,0,.2)}[dir=rtl] .ee-x-corner>b,.ee-x-corner>b{transform:rotate(45deg)}
.ee-x-hcard{display:grid;grid-template-columns:minmax(110px,38%) 1fr;gap:0;background:var(--card);border:1px solid var(--line);border-radius:var(--radius);overflow:hidden;margin:1em 0}.ee-x-hcard img{width:100%;height:100%;object-fit:cover;min-height:130px}.ee-x-hcard div{padding:16px 18px}.ee-x-hcard h3{margin:0 0 .3em}.ee-x-hcard p{margin:0 0 .5em;color:var(--muted)}
.ee-x-split{display:grid;grid-template-columns:repeat(auto-fit,minmax(260px,1fr));gap:28px;align-items:center;margin:2em 0}.ee-x-split img{width:100%;border-radius:18px}.ee-x-split.rev>img{order:2}.ee-x-split h2{margin:0 0 .3em}
.ee-x-quote{margin:1.2em 0;padding:.6em 1.2em;border-inline-start:4px solid var(--accent);background:color-mix(in srgb,var(--accent) 7%,var(--card));border-radius:0 12px 12px 0;font-size:1.05rem}
.ee-x-wa{position:fixed;inset-block-end:18px;inset-inline-start:18px;z-index:40;display:inline-flex;gap:.5em;align-items:center;padding:.7em 1.2em;border-radius:99px;background:#25d366;color:#fff;font-weight:800;text-decoration:none;box-shadow:0 8px 22px rgba(37,211,102,.45)}
.ee-x-map{position:relative;aspect-ratio:16/10;border-radius:16px;overflow:hidden;margin:1em 0;background:var(--line)}.ee-x-map iframe{position:absolute;inset:0;width:100%;height:100%;border:0}
.ee-x-cookie{display:flex;flex-wrap:wrap;gap:12px;align-items:center;justify-content:space-between;background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 18px;margin:1em 0;box-shadow:0 10px 30px rgba(0,0,0,.12)}.ee-x-cookie p{margin:0;flex:1;min-width:200px}
.ee-x-ba{display:grid;grid-template-columns:1fr 1fr;gap:6px;margin:1em 0}.ee-x-ba figure{margin:0;position:relative}.ee-x-ba img{width:100%;border-radius:12px;display:block}.ee-x-ba figcaption{position:absolute;top:8px;inset-inline-start:8px;background:rgba(0,0,0,.65);color:#fff;padding:.1em .8em;border-radius:99px;font-size:.8rem}
.ee-x-read{display:inline-flex;gap:.4em;align-items:center;color:var(--muted);font-size:.88rem}
.ee-x-event{display:flex;gap:16px;align-items:center;background:var(--card);border:1px solid var(--line);border-radius:var(--radius);padding:14px 18px;margin:1em 0}.ee-x-event .d{flex:none;width:64px;text-align:center;border-radius:12px;background:var(--accent);color:var(--accent-fg);padding:.4em 0;line-height:1.2}.ee-x-event .d b{display:block;font-size:1.6rem}.ee-x-event .d small{font-size:.8rem}.ee-x-event h3{margin:0}.ee-x-event p{margin:.1em 0 0;color:var(--muted)}
)CSS"},
};

// ── سكربتات التشغيل (تُضمَّن عند وجود الصفة المقابلة) ──
struct WebJs { const char* trigger; const char* js; };
static const WebJs kWebJs[] = {
{"data-ee-tabs", "document.querySelectorAll('[data-ee-tabs]').forEach(function(t){var b=t.querySelectorAll('.tl button'),p=t.querySelectorAll('.tp');b.forEach(function(x,i){x.onclick=function(){b.forEach(function(y,j){y.classList.toggle('on',i==j);p[j].hidden=i!=j})}})});"},
{"data-ee-theme", "(function(){var r=document.documentElement,k='ee-theme';try{var s=localStorage.getItem(k);if(s)r.dataset.theme=s}catch(e){}document.querySelectorAll('[data-ee-theme]').forEach(function(b){b.onclick=function(){var d=r.dataset.theme?r.dataset.theme=='dark':matchMedia('(prefers-color-scheme:dark)').matches;r.dataset.theme=d?'light':'dark';try{localStorage.setItem(k,r.dataset.theme)}catch(e){}}})})();"},
{"data-ee-count", "document.querySelectorAll('[data-ee-count]').forEach(function(e){var to=parseFloat(e.dataset.eeCount),dec=(String(to).split('.')[1]||'').length,go=function(){var t0=null,f=function(t){t0=t0||t;var p=Math.min(1,(t-t0)/1400),v=to*(1-Math.pow(1-p,3));e.textContent=v.toLocaleString('ar-EG',{minimumFractionDigits:dec,maximumFractionDigits:dec});if(p<1)requestAnimationFrame(f)};requestAnimationFrame(f)};if('IntersectionObserver' in window){var o=new IntersectionObserver(function(x){if(x[0].isIntersecting){o.disconnect();go()}});o.observe(e)}else go()});"},
{"data-ee-countdown", "document.querySelectorAll('[data-ee-countdown]').forEach(function(e){var T=new Date(e.dataset.eeCountdown).getTime(),n=e.querySelectorAll('b'),tick=function(){var d=Math.max(0,T-Date.now())/1000,v=[Math.floor(d/86400),Math.floor(d%86400/3600),Math.floor(d%3600/60),Math.floor(d%60)];n.forEach(function(b,i){b.textContent=String(v[i]).padStart(2,'0')})};tick();setInterval(tick,1000)});"},
{"data-ee-copy", "document.querySelectorAll('[data-ee-copy]').forEach(function(b){b.onclick=function(){var t=b.dataset.eeCopy,o=b.textContent,d=function(){b.textContent='✔ تم النسخ';setTimeout(function(){b.textContent=o},1500)};if(navigator.clipboard)navigator.clipboard.writeText(t).then(d);else{var a=document.createElement('textarea');a.value=t;document.body.appendChild(a);a.select();document.execCommand('copy');a.remove();d()}}});"},
{"data-ee-open", "document.querySelectorAll('[data-ee-open]').forEach(function(b){b.onclick=function(){var d=document.getElementById(b.dataset.eeOpen);if(d&&d.showModal)d.showModal()}});"},
{"data-ee-reveal", "(function(){var e=document.querySelectorAll('[data-ee-reveal]');if(!('IntersectionObserver' in window)){e.forEach(function(x){x.classList.add('in')});return}var o=new IntersectionObserver(function(l){l.forEach(function(x){if(x.isIntersecting){x.target.classList.add('in');o.unobserve(x.target)}})},{threshold:.12});e.forEach(function(x){o.observe(x)})})();"},
};

