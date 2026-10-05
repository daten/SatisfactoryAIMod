export interface CatalogParam {
  name: string;
  type: string;
  required: boolean;
}

export interface CatalogMethod {
  method: string;
  category: "telemetry" | "command" | "build" | string;
  summary: string;
  params: CatalogParam[];
  creative?: boolean;
}

export interface Catalog {
  protocolVersion: number;
  methodCount: number;
  methods: CatalogMethod[];
}
