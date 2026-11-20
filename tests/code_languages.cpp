#include <cyui/modules/Code.h>
#ifdef CYUI_HAS_DIFF
#include <cyui/modules/Diff.h>
#endif
#ifdef CYUI_HAS_CODEEDITOR
#include <cyui/modules/CodeEditor.h>
#include <TextEditor.h>
#endif
#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#include <stdexcept>
#include <thread>
using namespace cyui;
static void check(bool ok,const std::string &message) { if (!ok) throw std::runtime_error(message); }
static TokenRole role(std::string_view source,CodeLanguage language,std::string_view word) {
    const auto offset=source.find(word);check(offset!=source.npos,"Missing fixture token");
    for (const auto &token : highlightCode(source,language))
        if (token.start<=offset && offset<token.start+token.length) return token.role;
    return TokenRole::text;
}
static void catalogue() {
    const std::pair<CodeLanguage,const char*> fixtures[]={
#include "CodeLanguageSamples.inc"
    };
    const auto items=codeLanguages();check(items.size()==104,"Expected 104 built-in syntax modes");
    check(std::size(fixtures)==items.size(),"Each built-in mode needs an independent fixture");
    std::set<std::string> ids;std::set<CodeLanguage> values;
    for (const auto &item : items) {
        check(ids.insert(item.id).second && values.insert(item.language).second,"Duplicate built-in syntax identity");
        check(codeLanguageFromName(item.id)==item.language,"Registry lookup failed: "+item.id);
    }
    int missing=0;
    for (const auto &fixture : fixtures) {
        const std::string source=fixture.second;const auto info=codeLanguageInfo(fixture.first);
        const auto tokens=highlightCode(source,fixture.first);CodeDocument doc;doc.setText(source,fixture.first,3);
        check(doc.text()==source,"Lexer changed source bytes: "+info.id);
        std::size_t end=0;bool colored=false;
        for (const auto &token : tokens) {
            check(token.start>=end && token.length && token.length<=source.size()-token.start,"Invalid token range: "+info.id);
            end=token.start+token.length;colored|=token.role!=TokenRole::text;
        }
        if (!colored && fixture.first!=CodeLanguage::plain && fixture.first!=CodeLanguage::nfo) {
            std::cerr<<"No colored token for "<<info.id<<'\n';++missing;
        }
    }
    check(!missing,"One or more built-in engines did not color their fixture");
    for (const auto &sample : std::vector<std::pair<std::string,CodeLanguage>>{
        {"CMakeLists.txt",CodeLanguage::cmake},{"GNUmakefile",CodeLanguage::makefile},
        {".bashrc",CodeLanguage::shell},{"Cargo.lock",CodeLanguage::toml},
        {"some/PATH/FILE.HLSLI",CodeLanguage::hlsl},{"shader.comp",CodeLanguage::glsl},
        {"plugin.uplugin",CodeLanguage::json},{"some/code.ps1",CodeLanguage::powershell},
        {"Widget.java",CodeLanguage::java},{"Widget.tsx",CodeLanguage::tsx}})
        check(codeLanguageForPath(sample.first)==sample.second,"Filename detection failed: "+sample.first);
    check(role("procedure Demo is begin null; end Demo;",CodeLanguage::ada,"procedure")==TokenRole::keyword,"Ada keyword profile");
    check(role("func main() { return }",CodeLanguage::go,"func")==TokenRole::keyword,"Go keyword profile");
    check(role("let value = 42",CodeLanguage::swift,"let")==TokenRole::keyword,"Swift keyword profile");
    check(role("SELECT value FROM table_name",CodeLanguage::sql,"SELECT")==TokenRole::keyword,"SQL keyword profile");
    const std::string hlsl="cbuffer Camera { float4x4 View; }; float4 Main() : SV_Target { return mul(View,float4(1,2,3,1)); }";
    check(role(hlsl,CodeLanguage::hlsl,"cbuffer")==TokenRole::keyword,"HLSL storage keyword");
    check(role(hlsl,CodeLanguage::hlsl,"float4x4")==TokenRole::type,"HLSL matrix type");
    check(role(hlsl,CodeLanguage::hlsl,"mul")==TokenRole::function,"HLSL intrinsic");
    const std::string glsl="#version 450\nlayout(location=0) in vec3 pos; void main() { gl_Position=vec4(pos,1.0); }";
    check(role(glsl,CodeLanguage::glsl,"vec3")==TokenRole::type,"GLSL vector type");
    check(role(glsl,CodeLanguage::glsl,"layout")==TokenRole::keyword,"GLSL layout qualifier");
    check(role("-- note\nSELECT 42",CodeLanguage::sql,"note")==TokenRole::comment,"SQL comments");
    check(role("old\n-new\n+added\n",CodeLanguage::diff,"added")==TokenRole::added,"Diff added role");
    CodeDocument source;source.setText("\tfloat café=42;\r\n",CodeLanguage::cpp,8);
    check(source.text()=="\tfloat café=42;\r\n" && source.lines()[0].text.rfind("    float",0)==0,"UTF-8/tabs/CRLF round trip");
    std::string large;for (int i=0;i<10000;++i) large+="float4 Main(float3 pos) { return float4(pos,1.0); }\n";
    const auto start=std::chrono::steady_clock::now();highlightCode(large,CodeLanguage::hlsl);
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
    check(elapsed<5000,"Shader lexer exceeded five seconds for 10,000 lines");
    std::cout<<items.size()<<" syntax fixtures; HLSL 10,000 lines: "<<elapsed<<" ms\n";
}
static void extensions() {
    const auto builtinCount=codeLanguages().size();
    CodeLanguageDefinition definition;definition.id="test/cyconfig";definition.name="CyConfig";definition.extensions={"cycfg"};
    CodeSyntaxRules rules;rules.keywords={"option"};rules.types={"number"};rules.lineComment="#";
    rules.blockCommentStart="/*";rules.blockCommentEnd="*/";definition.rules=rules;
    const auto before=codeLanguageRevision();const auto language=registerCodeLanguage(definition);
    check(codeLanguageRevision()>before && codeLanguageForPath("settings.cycfg")==language,"Plugin association registration");
    const std::string text="option number count=42 # note\n/* multi\nline */\n";
    check(role(text,language,"option")==TokenRole::keyword && role(text,language,"number")==TokenRole::type &&
          role(text,language,"note")==TokenRole::comment && role(text,language,"line")==TokenRole::comment,"Declarative plugin lexer");
    CodeDocument cached;cached.setText(text,language,9);const auto old=cached.lines()[0].spans;
    definition.rules->keywords={"other"};check(registerCodeLanguage(definition)==language,"Plugin update changed its stable ID");
    check(cached.setText(text,language,9),"Registry update did not invalidate document syntax");
    check(role(text,language,"option")==TokenRole::text,"Old plugin keyword survived update");
    CodeLanguageDefinition inherited;inherited.id="test/derived";inherited.name="Derived C++";inherited.base=CodeLanguage::cpp;
    inherited.wordLists={"special"};const auto derived=registerCodeLanguage(inherited);
    check(role("special value;",derived,"special")==TokenRole::keyword,"Inherited keyword set was not applied");
    inherited.base=CodeLanguage::plain;inherited.wordLists.resize(9);registerCodeLanguage(inherited);
    check(role("safe plain",derived,"safe")==TokenRole::text,"Plain base with word sets was unsafe");
    unregisterCodeLanguage(inherited.id);
    CodeLanguageDefinition callback;callback.id="test/custom";callback.name="Custom parser";
    callback.highlight=[](std::string_view source) { return std::vector<CodeToken>{{0,source.size(),TokenRole::type}}; };
    const auto custom=registerCodeLanguage(callback);check(role("custom",custom,"custom")==TokenRole::type,"Plugin engine callback");
    CodeDocument safe;safe.setText("before");
    callback.highlight=[](std::string_view) { return std::vector<CodeToken>{{1,1,TokenRole::type}}; };registerCodeLanguage(callback);
    bool rejected=false;try { safe.setText("🛠",custom); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected && safe.text()=="before","Invalid callback span split UTF-8 or replaced the existing document");
    callback.id="cpp";rejected=false;try { registerCodeLanguage(callback); } catch (const std::invalid_argument&) { rejected=true; }
    check(rejected,"A plugin replaced a built-in language");
    check(unregisterCodeLanguage("test/custom") && unregisterCodeLanguage("test/cyconfig"),"Plugin removal");
    check(codeLanguageForPath("settings.cycfg")==CodeLanguage::plain && codeLanguages().size()==builtinCount,"Plugin removal did not restore catalogue");
    check(cached.setText(text,language,9),"Removing a plugin left stale cached syntax");
#ifdef CYUI_HAS_CODEEDITOR
    CodeEditor editor;editor.setLanguage(CodeLanguage::hlsl);editor.setText("float4 Main() { return float4(1,2,3,4); }");editor.refreshSyntax();
    check(editor.native().GetSharedSyntaxColor({0,0})==TextEditor::Color::declaration,"Common editor did not use the shared lexer");
    const auto original=editor.text();const auto version=editor.native().GetUndoIndex();
    editor.refreshSyntax();check(editor.text()==original && editor.native().GetUndoIndex()==version,"Syntax changed text or Undo");
    editor.native().SelectAll();editor.native().ReplaceTextInCurrentCursor("float4 Other;");editor.refreshSyntax();
    editor.native().Undo();editor.refreshSyntax();check(editor.text()==original,"Common syntax broke Undo");
    editor.native().Redo();editor.refreshSyntax();check(editor.text()=="float4 Other;","Common syntax broke Redo");
    editor.native().SetText("float4 Reset;");editor.refreshSyntax();
    check(editor.native().GetSharedSyntaxColor({0,0})==TextEditor::Color::declaration,"Native reset reused a stale syntax revision");
#endif
}
static void logs() {
    const std::string source="2026-11-02 [INFO] Started café 🛠\r\n"
        "[Error] first failure\r\nLogTemp: Warning: missing value\n"
        "{\"level\":\"error\",\"message\":\"JSON failure\"}\n"
        "[warn] lower case\nCritical failure\nFatal exception\n"
        "[DEBUG] diagnostic detail\nLogTemp: VeryVerbose: detail\n"
        "Normal ErrorCode and WarningCount and traceback stay neutral\n"
        "Warning before ERROR wins red\n\x1b[31mERROR\x1b[0m ANSI logger\n";
    check(codeLanguageForPath("build.LOG")==CodeLanguage::log,"Log extension not detected");
    CodeDocument doc;doc.setText(source,CodeLanguage::log,1);
    const TokenRole expected[]={TokenRole::text,TokenRole::error,TokenRole::warning,TokenRole::error,
        TokenRole::warning,TokenRole::error,TokenRole::error,TokenRole::comment,TokenRole::comment,
        TokenRole::text,TokenRole::error,TokenRole::error};
    for (std::size_t line=0;line<std::size(expected);++line) {
        const auto &row=doc.lines()[line];
        check(row.spans.size()==1 && row.spans[0].start==0 && row.spans[0].length==row.text.size() &&
              row.spans[0].role==expected[line],"Log severity did not color the whole row "+std::to_string(line));
    }
    check(doc.text()==source,"Log syntax changed UTF-8, ANSI bytes or CRLF");
    CodeLanguageDefinition plugin;plugin.id="test/log-style";plugin.name="Engine log";plugin.base=CodeLanguage::log;plugin.extensions={"engine-log"};
    const auto custom=registerCodeLanguage(plugin);
    check(role("LogTemp: Error: shader",custom,"shader")==TokenRole::error,"Plugin did not inherit log severity");
    unregisterCodeLanguage(plugin.id);
#ifdef CYUI_HAS_CODEEDITOR
    CodeEditor editor;editor.setLanguage(CodeLanguage::log);editor.setText("Error: issue\nWarning: note\nNormal message");editor.refreshSyntax();
    check(editor.native().GetSharedSyntaxColor({0,10})==TextEditor::Color::sharedError &&
          editor.native().GetSharedSyntaxColor({1,10})==TextEditor::Color::sharedWarning &&
          editor.native().GetSharedSyntaxColor({2,0})==TextEditor::Color::text,"Common editor log colors did not match whole-line levels");
    const auto before=editor.text();editor.native().SelectAll();editor.native().ReplaceTextInCurrentCursor("Warning: edited");editor.refreshSyntax();
    check(editor.native().GetSharedSyntaxColor({0,0})==TextEditor::Color::sharedWarning,"Log edit kept stale severity");
    editor.native().Undo();editor.refreshSyntax();check(editor.text()==before && editor.native().GetSharedSyntaxColor({0,0})==TextEditor::Color::sharedError,"Log Undo lost bytes or severity");
#endif
}
static void verse() {
    check(codeLanguageFromName("Verse")==CodeLanguage::verse &&
          codeLanguageForPath("Content/Devices/MyDevice.VERSE")==CodeLanguage::verse &&
          codeLanguageForPath("Verse.digest.verse")==CodeLanguage::verse,"Verse identity/associations");
    const std::string text="using { /Verse.org/Simulation }\r\n"
        "<# outer <# nested #> still comment #>\r\n"
        "counter := class:\r\n    @editable\r\n    var Score:int = 42\r\n"
        "    OnBegin<override>()<suspends>:void = Print(\"Score {Score + 1}\")\r\n";
    check(role(text,CodeLanguage::verse,"using")==TokenRole::keyword,"Verse keyword");
    check(role(text,CodeLanguage::verse,"nested")==TokenRole::comment &&
          role(text,CodeLanguage::verse,"still")==TokenRole::comment,"Nested Verse comment");
    check(role(text,CodeLanguage::verse,"@editable")==TokenRole::preprocessor,"Verse attribute");
    check(role(text,CodeLanguage::verse,"int")==TokenRole::type,"Verse primitive type");
    check(role(text,CodeLanguage::verse,"OnBegin")==TokenRole::function,"Verse modified function");
    const auto source=text+"# UTF-8: café 🛠\n";
    CodeDocument document;document.setText(source,CodeLanguage::verse,5);
    check(document.text()==source,"Verse syntax changed UTF-8/CRLF bytes");
    CodeLanguageDefinition plugin;plugin.id="test/verse";plugin.name="Verse dialect";
    plugin.base=CodeLanguage::verse;plugin.extensions={"verse"};plugin.wordLists={"custom"};
    const auto derived=registerCodeLanguage(plugin);
    check(codeLanguageForPath("MyDevice.verse")==derived,"Verse plugin association");
    check(role("custom Counter:int",derived,"custom")==TokenRole::keyword &&
          role("custom Counter:int",derived,"int")==TokenRole::type,"Inherited Verse word families");
    check(role("<# a <# b #> c #>",derived,"c")==TokenRole::comment,"Plugin inherited native Verse scanner");
    unregisterCodeLanguage(plugin.id);
    check(codeLanguageForPath("MyDevice.verse")==CodeLanguage::verse,"Verse association restoration");
#ifdef CYUI_HAS_CODEEDITOR
    CodeEditor editor;editor.setLanguageForPath("device.verse");editor.setText(source);editor.refreshSyntax();
    check(editor.native().GetSharedSyntaxColor({0,0})==TextEditor::Color::keyword,"Verse editor shared keyword");
    editor.native().SelectAll();editor.native().ReplaceTextInCurrentCursor("var Value:int = 7");editor.refreshSyntax();
    editor.native().Undo();editor.refreshSyntax();check(editor.text()==source,"Verse Undo lost source bytes");
    editor.native().Redo();editor.refreshSyntax();check(editor.text()=="var Value:int = 7","Verse Redo failed");
#endif
}
static void zigAndZon() {
    const auto zig = CodeLanguage::zig, zon = CodeLanguage::zon;
    static_assert(unsigned(CodeLanguage::verse) == 101 && unsigned(CodeLanguage::zig) == 102 &&
                  unsigned(CodeLanguage::zon) == 103, "Persisted language IDs are append-only");
    check(codeLanguageFromName("Zig") == zig && codeLanguageFromName("ZON") == zon, "Zig/ZON IDs");
    check(codeLanguageForPath("src/Voice.ZIG") == zig && codeLanguageForPath("build.zig") == zig &&
          codeLanguageForPath("data.zon") == zon && codeLanguageForPath("pkg\\BUILD.ZIG.ZON") == zon &&
          codeLanguageForPath("build.zig.zon.bak") == CodeLanguage::plain, "Zig/ZON file associations");

    // Check every byte of each marker, including the end of an escape or signed exponent.
    auto expect = [](const std::string &source, CodeLanguage language,
                     std::initializer_list<std::pair<std::string_view, TokenRole>> markers) {
        const auto tokens = highlightCode(source, language);
        for (const auto &[marker, expected] : markers) {
            const auto begin = source.find(marker);
            check(begin != source.npos, "Missing Zig/ZON marker: " + std::string(marker));
            const auto token = std::find_if(tokens.begin(), tokens.end(), [&](const CodeToken &t) {
                return t.start <= begin && t.start + t.length >= begin + marker.size();
            });
            check(token != tokens.end() && token->role == expected, "Zig/ZON role: " + std::string(marker));
        }
    };
    using R = TokenRole;
    const std::string source =
        "//! @notCode( café\r\n/// fn docs\n// 0xbeef\n"
        "const Packet = packed struct { tag: u7, delta: i37 };\n"
        "pub fn @\"réglage\"(bytes: []const u8) !void {\n"
        "\tconst a: comptime_int = 0b10_01 + 0o71 + 0xfA_Ce;\n"
        "\tconst b: f64 = 0x1.fp-3 + 12.5e+2;\n"
        "\tfor (bytes[0..2]) |v| { use(v +% 1); }\n"
        "\tconst c = '\\u{1f3b5}'; const s = \"\\\"// @as\\x21\";\n"
        "\tconst raw =\r\n\t    \\\\literal // @abs(\"é\")\r\n\t    \\\\again \\n\n\t;\n"
        "\t_ = @as(usize, @intCast(a)); _ = @\"fn\";\n"
        "\tif (true) return error.Empty;\n}\n";
    expect(source, zig, {{"//! @notCode( café",R::comment},{"/// fn docs",R::comment},{"// 0xbeef",R::comment},
        {"Packet",R::type},{"packed",R::keyword},{"u7",R::type},{"i37",R::type},{"@\"réglage\"",R::function},
        {"comptime_int",R::type},{"0b10_01",R::number},{"0o71",R::number},{"0xfA_Ce",R::number},
        {"0x1.fp-3",R::number},{"12.5e+2",R::number},{"..",R::text},{"+%",R::text},{"use",R::function},
        {"'\\u{1f3b5}'",R::string},{"\"\\\"// @as\\x21\"",R::string},{"\\\\literal // @abs(\"é\")",R::string},
        {"\\\\again \\n",R::string},{"@intCast",R::function},{"@\"fn\"",R::text},{"true",R::keyword},{"error",R::keyword}});
    expect("i7field True inf class @\"name\\\"with quote\"", zig,
           {{"i7field",R::text},{"True",R::text},{"inf",R::text},{"class",R::text},{"@\"name\\\"with quote\"",R::text}});
    expect("//\nconst a = \"unfinished @as\nfn next() u0 { return 0; }", zig,
           {{"const",R::keyword},{"unfinished @as",R::string},{"next",R::function},{"u0",R::type}});
    expect("fn @\"clé\\\"🙂\"() void {} // 🎵", zig,
           {{"@\"clé\\\"🙂\"",R::function},{"void",R::type},{"// 🎵",R::comment}});
    expect(".{ .name = .synth, .nan = nan, .v = .{ -inf, true, null, 'é', \"//msg\" } }", zon,
           {{".synth",R::text},{".nan",R::text},{"inf",R::number},{"true",R::keyword},{"null",R::keyword},
            {"'é'",R::string},{"\"//msg\"",R::string}});
    expect("nan", zon, {{"nan",R::number}});
    expect("const fn u7 bool undefined @import(42)", zon,
           {{"const",R::text},{"fn",R::text},{"u7",R::text},{"bool",R::text},{"undefined",R::text},{"@import",R::text}});
    CodeDocument doc;
    check(doc.setText(source, zig, 1) && doc.text() == source && !doc.setText(source, zig, 1),
          "Zig viewer cache or UTF-8/CRLF round trip");
    std::string large;
    for (int line = 0; line < 10000; ++line) large += "    \\\\音 // fn @as\r\n";
    large += "; const last: u24 = 1;";
    expect(large, zig, {{"音 // fn @as",R::string},{"const",R::keyword},{"u24",R::type}});

    CodeLanguageDefinition ext;
    ext.id = "test/zig-dialect"; ext.name = "Zig dialect"; ext.base = zig;
    ext.extensions = {"zig"}; ext.filenames = {"manifest.zon"}; ext.wordLists = {"custom fn"};
    const auto custom = registerCodeLanguage(ext);
    check(codeLanguageForPath("build.zig") == custom && codeLanguageForPath("manifest.zon") == custom &&
          codeLanguageForPath("build.zig.zon") == zon, "Zig plugin association priority");
    expect("custom const usize", custom, {{"custom",R::keyword},{"const",R::text},{"usize",R::type}});
    expect("const", zig, {{"const",R::keyword}});
    ext.base = zon; ext.wordLists = {}; registerCodeLanguage(ext);
    check(role("nan",custom,"nan") == R::number && role("u7",custom,"u7") == R::text, "Inherited ZON");
    ext.rules.emplace(); ext.rules->types = {"nan"}; registerCodeLanguage(ext);
    check(role("nan",custom,"nan") == R::type, "Declarative rules override the ZON base");
    ext.highlight = [](std::string_view text) { return std::vector<CodeToken>{{0,text.size(),R::comment}}; };
    registerCodeLanguage(ext);
    check(role("nan",custom,"nan") == R::comment, "Plugin callback overrides declarative rules");
    unregisterCodeLanguage(ext.id);
    check(codeLanguageForPath("manifest.zon") == zon && codeLanguageForPath("build.zig") == zig, "Zig/ZON restoration");
#ifdef CYUI_HAS_CODEEDITOR
    CodeEditor editor;
    editor.setLanguageForPath("build.zig.zon"); editor.setText("nan"); editor.refreshSyntax();
    using C = TextEditor::Color;
    auto color = [&](std::size_t line, std::size_t column) { return editor.native().GetSharedSyntaxColor({line, column}); };
    check(color(0,0) == C::number, "ZON editor detection");
    editor.setLanguage(zig); editor.refreshSyntax();
    check(color(0,0) == C::text, "Manual Zig selection on a ZON path");
    editor.native().SelectAll(); editor.native().ReplaceTextInCurrentCursor("\\\\é // fn\nconst a: u7 = 3;");
    editor.refreshSyntax();
    check(color(0,3) == C::string && color(1,0) == C::keyword && color(1,9) == C::declaration, "Edited Zig spans");
    editor.native().Undo(); editor.refreshSyntax();
    check(editor.text() == "nan" && color(0,0) == C::text, "Undo after a Zig edit");
    editor.native().Redo(); editor.refreshSyntax();
    check(color(0,3) == C::string && color(1,9) == C::declaration, "Redo after a Zig edit");
#endif
#ifdef CYUI_HAS_DIFF
    DiffDocument diff;
    diff.setTexts("const a = 1;", "const a = @abs(x);", zig);
    bool found = false;
    for (const auto &row : diff.rows()) if (row.kind == DiffKind::added)
        for (const auto &span : row.code.spans)
            found |= span.role == R::function && row.code.text.substr(span.start, span.length) == "@abs";
    check(found, "Zig diff did not use the shared builtin-function role");
    diff.setUnified("@@ -1 +1 @@\n-inf\n+nan\n", zon);
    int changes = 0;
    for (const auto &row : diff.rows()) if (row.kind == DiffKind::added || row.kind == DiffKind::removed) {
        check(row.code.spans.size() == 1 && row.code.spans.front().role == R::number, "ZON patch literal");
        ++changes;
    }
    check(changes == 2, "Both sides of the ZON patch must be prepared");
#endif
}
int main() {
    ImGui::CreateContext();
    try {
        check(role("// TODO keep this comment green\n",CodeLanguage::cpp,"TODO")==TokenRole::comment,"TODO marker escaped the common comment role");
        catalogue();extensions();logs();verse();zigAndZon();ImGui::DestroyContext();std::cout<<"Shared catalogue, Zig/ZON, shader types/intrinsics, plugin registration/rules/callbacks/removal and editor revisions passed\n";return 0; }
    catch (const std::exception &error) { std::cerr<<error.what()<<'\n';ImGui::DestroyContext();return 1; }
}
