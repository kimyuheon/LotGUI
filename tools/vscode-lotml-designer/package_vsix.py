#!/usr/bin/env python3
"""Package the dependency-free LotUI Designer as a VSIX."""

import argparse
import json
from pathlib import Path
import xml.etree.ElementTree as ET
from zipfile import ZIP_DEFLATED, ZipFile


def element(parent, name, text=None, **attributes):
    child = ET.SubElement(parent, name, attributes)
    child.text = text
    return child


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output_directory", type=Path)
    args = parser.parse_args()
    source = Path(__file__).resolve().parent
    package = json.loads((source / "package.json").read_text(encoding="utf-8"))

    manifest = ET.Element("PackageManifest", {
        "Version": "2.0.0",
        "xmlns": "http://schemas.microsoft.com/developer/vsx-schema/2011",
    })
    metadata = element(manifest, "Metadata")
    element(metadata, "Identity", Id=package["name"], Publisher=package["publisher"],
            Version=package["version"], Language="en-US")
    element(metadata, "DisplayName", package["displayName"])
    element(metadata, "Description", package["description"])
    element(metadata, "Categories", ",".join(package["categories"]))
    properties = element(metadata, "Properties")
    element(properties, "Property", Id="Microsoft.VisualStudio.Code.Engine",
            Value=package["engines"]["vscode"])
    installation = element(manifest, "Installation")
    element(installation, "InstallationTarget", Id="Microsoft.VisualStudio.Code")
    element(manifest, "Dependencies")
    assets = element(manifest, "Assets")
    for name, file in (
        ("Microsoft.VisualStudio.Code.Manifest", "package.json"),
        ("Microsoft.VisualStudio.Services.Content.Details", "README.md"),
        ("Microsoft.VisualStudio.Services.Content.License", "LICENSE.txt"),
    ):
        element(assets, "Asset", Type=name, Path=f"extension/{file}", Addressable="true")

    types = ET.Element("Types", {
        "xmlns": "http://schemas.openxmlformats.org/package/2006/content-types",
    })
    for extension, mime in (
        ("vsixmanifest", "text/xml"), ("json", "application/json"),
        ("js", "application/javascript"), ("css", "text/css"),
        ("md", "text/markdown"), ("txt", "text/plain"),
    ):
        element(types, "Default", Extension=extension, ContentType=mime)

    files = (
        "extension.js", "model.js", "package.json", "schema.json",
        "language-configuration.json", "lotml.tmLanguage.json", "README.md",
        "media/designer.css", "media/designer.js",
    )
    args.output_directory.mkdir(parents=True, exist_ok=True)
    output = args.output_directory / f'{package["name"]}-{package["version"]}.vsix'
    with ZipFile(output, "w", compression=ZIP_DEFLATED) as archive:
        archive.writestr("extension.vsixmanifest",
                         ET.tostring(manifest, encoding="utf-8", xml_declaration=True))
        archive.writestr("[Content_Types].xml",
                         ET.tostring(types, encoding="utf-8", xml_declaration=True))
        for file in files:
            archive.write(source / file, f"extension/{file}")
        archive.write(source.parent.parent / "LICENSE", "extension/LICENSE.txt")
    print(output)


if __name__ == "__main__":
    main()
