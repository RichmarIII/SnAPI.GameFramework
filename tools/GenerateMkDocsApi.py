#!/usr/bin/env python3
import argparse
import shutil
import textwrap
import xml.etree.ElementTree as ElementTree
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple


@dataclass
class Compound:
    refid: str
    kind: str
    name: str


class DoxygenApiGenerator:
    def __init__(self, xml_dir: Path, out_dir: Path) -> None:
        self._xml_dir = xml_dir
        self._out_dir = out_dir
        self._compounds: List[Compound] = []

    def Run(self) -> None:
        self._LoadIndex()
        self._PrepareOutput()
        self._WriteCompoundPages()
        self._WriteIndexes()

    def _LoadIndex(self) -> None:
        index_path = self._xml_dir / "index.xml"
        if not index_path.exists():
            raise FileNotFoundError(f"Missing Doxygen index: {index_path}")
        tree = ElementTree.parse(index_path)
        root = tree.getroot()
        for compound in root.findall("compound"):
            kind = compound.get("kind", "")
            refid = compound.get("refid", "")
            name_elem = compound.find("name")
            name = name_elem.text.strip() if name_elem is not None and name_elem.text else refid
            if not refid:
                continue
            self._compounds.append(Compound(refid=refid, kind=kind, name=name))

    def _PrepareOutput(self) -> None:
        if self._out_dir.exists():
            shutil.rmtree(self._out_dir)
        self._out_dir.mkdir(parents=True, exist_ok=True)

    def _WriteCompoundPages(self) -> None:
        for compound in self._compounds:
            if compound.kind in {"class", "struct", "namespace", "file", "union", "concept"}:
                content = self._RenderCompound(compound)
                (self._out_dir / f"{compound.refid}.md").write_text(content, encoding="utf-8")

    def _WriteIndexes(self) -> None:
        namespaces = sorted([c for c in self._compounds if c.kind == "namespace"], key=lambda c: c.name)
        types = sorted(
            [c for c in self._compounds if c.kind in {"class", "struct", "union", "concept"}],
            key=lambda c: c.name,
        )
        files = sorted([c for c in self._compounds if c.kind == "file"], key=lambda c: c.name)

        def render_list(items: Iterable[Compound]) -> str:
            lines = []
            for item in items:
                lines.append(f"- [{item.name}]({item.refid}.md)")
            return "\n".join(lines) if lines else "_None_"

        index_content = textwrap.dedent(
            f"""\
            # API Reference

            This section is generated from Doxygen XML output and rendered inside MkDocs.

            - **Namespaces:** {len(namespaces)}
            - **Types:** {len(types)}
            - **Files:** {len(files)}

            ## Quick Index

            - [Namespaces](namespaces.md)
            - [Types](types.md)
            - [Files](files.md)
            """
        )
        (self._out_dir / "index.md").write_text(index_content, encoding="utf-8")

        namespaces_content = "# Namespaces\n\n" + render_list(namespaces) + "\n"
        types_content = "# Types\n\n" + render_list(types) + "\n"
        files_content = "# Files\n\n" + render_list(files) + "\n"

        (self._out_dir / "namespaces.md").write_text(namespaces_content, encoding="utf-8")
        (self._out_dir / "types.md").write_text(types_content, encoding="utf-8")
        (self._out_dir / "files.md").write_text(files_content, encoding="utf-8")

    def _RenderCompound(self, compound: Compound) -> str:
        xml_path = self._xml_dir / f"{compound.refid}.xml"
        if not xml_path.exists():
            return f"# {compound.name}\n\n_Unable to locate XML for {compound.refid}._\n"

        tree = ElementTree.parse(xml_path)
        root = tree.getroot()
        compounddef = root.find("compounddef")
        if compounddef is None:
            return f"# {compound.name}\n\n_Empty compound definition._\n"

        title = compound.name
        if compound.kind == "file":
            title = f"File `{compound.name}`"

        brief = self._RenderDescription(compounddef.find("briefdescription"))
        detailed = self._RenderDescription(compounddef.find("detaileddescription"))
        contents = self._RenderInner(compounddef)
        members = self._RenderSections(compounddef)

        parts = [f"# {title}", ""]
        if brief:
            parts.append(brief)
            parts.append("")
        if detailed:
            parts.append(detailed)
            parts.append("")
        if contents:
            parts.append("## Contents")
            parts.append("")
            parts.append(contents)
            parts.append("")
        if members:
            parts.append(members)
        return "\n".join(parts).strip() + "\n"

    def _RenderInner(self, compounddef: ElementTree.Element) -> str:
        lines = []
        for tag, label in (("innernamespace", "Namespace"), ("innerclass", "Type")):
            for inner in compounddef.findall(tag):
                name = inner.text.strip() if inner.text else None
                if not name:
                    continue
                lines.append(f"- **{label}:** {name}")
        return "\n".join(lines)

    def _RenderSections(self, compounddef: ElementTree.Element) -> str:
        output: List[str] = []
        for section in compounddef.findall("sectiondef"):
            kind = section.get("kind", "")
            title = self._SectionTitle(kind)
            members = section.findall("memberdef")
            rendered = self._RenderMembers(members)
            if rendered:
                output.append(f"## {title}")
                output.append("")
                output.append(rendered)
                output.append("")
        return "\n".join(output).strip()

    def _RenderMembers(self, members: Iterable[ElementTree.Element]) -> str:
        lines: List[str] = []
        for member in members:
            kind = member.get("kind", "")
            if kind in {"enum"}:
                lines.append(self._RenderEnum(member))
            else:
                lines.append(self._RenderMember(member))
        return "\n".join([line for line in lines if line])

    def _WrapCard(self, content: str) -> str:
        if not content:
            return ""
        return "\n".join(
            [
                '<div class="snapi-api-card" markdown="1">',
                content.strip(),
                "</div>",
            ]
        )

    def _RenderEnum(self, member: ElementTree.Element) -> str:
        name = member.findtext("name", "").strip()
        brief = self._RenderDescription(member.find("briefdescription"))
        detailed = self._RenderDescription(member.find("detaileddescription"))
        parts = [f"### `enum {name}`", ""]
        if brief:
            parts.append(brief)
            parts.append("")
        if detailed:
            parts.append(detailed)
            parts.append("")
        values = []
        for enum_value in member.findall("enumvalue"):
            value_name = enum_value.findtext("name", "").strip()
            value_brief = self._RenderDescription(enum_value.find("briefdescription"))
            if value_name:
                if value_brief:
                    values.append(f"- `{value_name}`: {value_brief}")
                else:
                    values.append(f"- `{value_name}`")
        if values:
            parts.append("**Values**")
            parts.append("")
            parts.append("\n".join(values))
            parts.append("")
        return self._WrapCard("\n".join(parts))

    def _RenderMember(self, member: ElementTree.Element) -> str:
        definition = member.findtext("definition", "").strip()
        args = member.findtext("argsstring", "").strip()
        if args:
            signature = f"{definition}{args}"
        else:
            signature = definition if definition else member.findtext("name", "").strip()

        brief = self._RenderDescription(member.find("briefdescription"))
        detailed = self._RenderDescription(member.find("detaileddescription"))
        params = self._ExtractParameters(member)
        returns = self._ExtractReturn(member)
        notes = self._ExtractNotes(member)

        parts = [f"### `{signature}`", ""]
        if brief:
            parts.append(brief)
            parts.append("")
        if detailed:
            parts.append(detailed)
            parts.append("")
        if params:
            parts.append("**Parameters**")
            parts.append("")
            parts.extend([f"- `{name}`: {desc}" for name, desc in params])
            parts.append("")
        if returns:
            parts.append(f"**Returns:** {returns}")
            parts.append("")
        if notes:
            parts.append("**Notes**")
            parts.append("")
            parts.extend([f"- {note}" for note in notes])
            parts.append("")
        return self._WrapCard("\n".join(parts).strip())

    def _ExtractParameters(self, member: ElementTree.Element) -> List[Tuple[str, str]]:
        descriptions: Dict[str, str] = {}
        detail = member.find("detaileddescription")
        if detail is not None:
            for item in detail.findall(".//parameterlist[@kind='param']/parameteritem"):
                names = [
                    n.text.strip()
                    for n in item.findall("./parameternamelist/parametername")
                    if n.text
                ]
                desc = self._RenderDescription(item.find("parameterdescription"))
                for name in names:
                    descriptions[name] = desc

        params: List[Tuple[str, str]] = []
        for param in member.findall("param"):
            name = param.findtext("declname") or param.findtext("defname") or ""
            if not name:
                continue
            params.append((name, descriptions.get(name, "")))
        return params

    def _ExtractReturn(self, member: ElementTree.Element) -> str:
        detail = member.find("detaileddescription")
        if detail is None:
            return ""
        return_desc = detail.find(".//simplesect[@kind='return']")
        return self._RenderDescription(return_desc)

    def _ExtractNotes(self, member: ElementTree.Element) -> List[str]:
        detail = member.find("detaileddescription")
        if detail is None:
            return []
        notes = []
        for note in detail.findall(".//simplesect[@kind='note']"):
            text = self._RenderDescription(note)
            if text:
                notes.append(text)
        return notes

    def _RenderDescription(self, elem: Optional[ElementTree.Element]) -> str:
        if elem is None:
            return ""
        paragraphs = []
        for child in elem:
            if child.tag == "para":
                text = self._RenderInline(child).strip()
                if text:
                    paragraphs.append(text)
            elif child.tag == "programlisting":
                paragraphs.append(self._RenderProgramlisting(child))
            elif child.tag == "itemizedlist":
                paragraphs.append(self._RenderList(child, ordered=False))
            elif child.tag == "orderedlist":
                paragraphs.append(self._RenderList(child, ordered=True))
        return "\n\n".join([p for p in paragraphs if p])

    def _RenderInline(self, elem: ElementTree.Element) -> str:
        parts: List[str] = []
        if elem.text:
            parts.append(elem.text)
        for child in elem:
            if child.tag in {"parameterlist", "simplesect"}:
                rendered = ""
            elif child.tag in {"computeroutput", "tt"}:
                rendered = f"`{self._RenderInline(child)}`"
            elif child.tag == "bold":
                rendered = f"**{self._RenderInline(child)}**"
            elif child.tag == "emphasis":
                rendered = f"*{self._RenderInline(child)}*"
            elif child.tag == "ref":
                rendered = self._RenderInline(child)
            elif child.tag == "ulink":
                label = self._RenderInline(child)
                url = child.get("url", "")
                rendered = f"[{label}]({url})" if url else label
            elif child.tag == "linebreak":
                rendered = "  \n"
            elif child.tag == "itemizedlist":
                rendered = "\n" + self._RenderList(child, ordered=False) + "\n"
            elif child.tag == "orderedlist":
                rendered = "\n" + self._RenderList(child, ordered=True) + "\n"
            else:
                rendered = self._RenderInline(child)

            parts.append(rendered)
            if child.tail:
                parts.append(child.tail)
        return "".join(parts)

    def _RenderProgramlisting(self, elem: ElementTree.Element) -> str:
        lines = []
        for codeline in elem.findall("codeline"):
            line = "".join(codeline.itertext())
            lines.append(line.rstrip())
        code = "\n".join(lines).rstrip()
        return f"```cpp\n{code}\n```"

    def _RenderList(self, elem: ElementTree.Element, ordered: bool) -> str:
        lines = []
        for idx, item in enumerate(elem.findall("listitem"), start=1):
            text = self._RenderInline(item).strip()
            prefix = f"{idx}." if ordered else "-"
            lines.append(f"{prefix} {text}")
        return "\n".join(lines)

    def _SectionTitle(self, kind: str) -> str:
        titles = {
            "public-func": "Public Functions",
            "public-static-func": "Public Static Functions",
            "public-attrib": "Public Members",
            "public-static-attrib": "Public Static Members",
            "public-type": "Public Types",
            "protected-func": "Protected Functions",
            "protected-attrib": "Protected Members",
            "protected-type": "Protected Types",
            "private-func": "Private Functions",
            "private-attrib": "Private Members",
            "private-type": "Private Types",
            "friend": "Friends",
            "define": "Macros",
            "func": "Functions",
            "var": "Variables",
            "typedef": "Type Aliases",
            "enum": "Enumerations",
        }
        return titles.get(kind, kind.replace("-", " ").title())


def Main() -> None:
    parser = argparse.ArgumentParser(description="Generate MkDocs API pages from Doxygen XML.")
    parser.add_argument("--xml-dir", required=True, help="Path to Doxygen XML output directory.")
    parser.add_argument("--out-dir", required=True, help="Output directory for MkDocs API markdown.")
    args = parser.parse_args()

    xml_dir = Path(args.xml_dir)
    out_dir = Path(args.out_dir)
    generator = DoxygenApiGenerator(xml_dir=xml_dir, out_dir=out_dir)
    generator.Run()


if __name__ == "__main__":
    Main()
