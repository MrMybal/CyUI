#include <cyui/modules/Code.h>
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
    const auto items=codeLanguages();check(items.size()==102,"Expected 102 built-in syntax modes");
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
    std::cout<<"102 syntax fixtures; HLSL 10,000 lines: "<<elapsed<<" ms\n";
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
int main() {
    ImGui::CreateContext();
    try {
        check(role("// TODO keep this comment green\n",CodeLanguage::cpp,"TODO")==TokenRole::comment,"TODO marker escaped the common comment role");
        catalogue();extensions();logs();verse();ImGui::DestroyContext();std::cout<<"Shared catalogue, shader types/intrinsics, plugin registration/rules/callbacks/removal and editor revisions passed\n";return 0; }
    catch (const std::exception &error) { std::cerr<<error.what()<<'\n';ImGui::DestroyContext();return 1; }
}
